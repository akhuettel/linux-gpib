/***************************************************************************
                          tnt4882_write.c  -  description
                             -------------------

    copyright            : (C) 2003 by Frank Mori Hess
    email                : fmhess@users.sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "tnt4882.h"
#include <linux/delay.h>

static int fifo_space_available( tnt4882_private_t *tnt_priv )
{
	int status2;
	int retval;

	status2 = tnt_readb( tnt_priv, STS2 );
	retval = ( status2 & AFFN ) && ( status2 & BFFN );

	return retval;
}

static int fifo_xfer_done( tnt4882_private_t *tnt_priv )
{
	int status1;
	int retval;

	status1 = tnt_readb( tnt_priv, STS1 );
	retval = status1 & ( S_DONE | S_HALT );

	return retval;
}

static int write_wait( gpib_board_t *board, tnt4882_private_t *tnt_priv,
	int wait_for_done )
{
	nec7210_private_t *nec_priv = &tnt_priv->nec7210_priv;

	if( wait_event_interruptible( board->wait,
		( !wait_for_done && fifo_space_available( tnt_priv ) ) ||
		fifo_xfer_done( tnt_priv ) ||
		test_bit( BUS_ERROR_BN, &nec_priv->state ) ||
		test_bit( DEV_CLEAR_BN, &nec_priv->state ) ||
		test_bit( TIMO_NUM, &board->status ) ) )
	{
		GPIB_DPRINTK( "gpib write interrupted\n" );
		return -ERESTARTSYS;
	}
	if( test_bit( TIMO_NUM, &board->status ) )
		return -ETIMEDOUT;
	if( test_bit( BUS_ERROR_BN, &nec_priv->state ) )
		return -EIO;
	if( test_bit( DEV_CLEAR_BN, &nec_priv->state ) )
		return -EINTR;

	return 0;
}

static ssize_t generic_write( gpib_board_t *board, uint8_t *buffer, size_t length,
	int send_eoi, int send_commands )
{
	size_t count = 0;
	ssize_t retval = 0;
	tnt4882_private_t *tnt_priv = board->private_data;
	nec7210_private_t *nec_priv = &tnt_priv->nec7210_priv;
	unsigned long iobase = nec_priv->iobase;
	unsigned int bits, imr1_bits, imr2_bits;
	int32_t hw_count;
	unsigned long flags;

	imr1_bits = nec_priv->reg_bits[ IMR1 ];
	imr2_bits = nec_priv->reg_bits[ IMR2 ];
	nec7210_set_reg_bits( nec_priv, IMR1, 0xff, HR_ERRIE | HR_DECIE );
	if( tnt_priv->chipset != TNT4882 )
		nec7210_set_reg_bits( nec_priv, IMR2, 0xff, HR_DMAO );
	else
		nec7210_set_reg_bits( nec_priv, IMR2, 0xff, 0 );

	tnt_writeb( tnt_priv, RESET_FIFO, CMDR );
	udelay(1);

	bits = TNT_TLCHE | TNT_B_16BIT;
	if( send_eoi )
	{
		bits |= TNT_CCEN;
		if( tnt_priv->chipset != TNT4882 )
			tnt_writeb( tnt_priv, AUX_SEOI, CCR );
	}
	if( send_commands )
		bits |= TNT_COMMAND;
	tnt_writeb( tnt_priv, bits, CFG );

	// load 2's complement of count into hardware counters
	hw_count = -length;
	tnt_writeb( tnt_priv, hw_count & 0xff, CNT0 );
	tnt_writeb( tnt_priv, ( hw_count >> 8 ) & 0xff, CNT1 );
	tnt_writeb( tnt_priv, ( hw_count >> 16 ) & 0xff, CNT2 );
	tnt_writeb( tnt_priv, ( hw_count >> 24 ) & 0xff, CNT3 );

	tnt_writeb( tnt_priv, GO, CMDR );
	udelay(1);

	spin_lock_irqsave( &board->spinlock, flags );
	tnt_priv->imr3_bits |= HR_DONE;
	tnt_writeb( tnt_priv, tnt_priv->imr3_bits, IMR3 );
	spin_unlock_irqrestore( &board->spinlock, flags );

	while( count < length  )
	{
		// wait until byte is ready to be sent
		retval = write_wait( board, tnt_priv, 0 );
		if( retval < 0 ) break;
		if( fifo_xfer_done( tnt_priv ) ) break;

		spin_lock_irqsave( &board->spinlock, flags );
		while( fifo_space_available( tnt_priv ) && count < length )
		{
			uint16_t word;

			word = buffer[ count++ ] & 0xff;
			if( count < length )
				word |= ( buffer[ count++ ] << 8 ) & 0xff00;
			//XXX not all boards use memory-mapped io
			tnt_priv->io_writew( word, iobase + FIFOB );
		}
		tnt_priv->imr3_bits |= HR_NFF;
		tnt_writeb( tnt_priv, tnt_priv->imr3_bits, IMR3 );
		spin_unlock_irqrestore( &board->spinlock, flags );

		if( current->need_resched )
			schedule();
	}
	// wait last byte has been sent
	retval = write_wait( board, tnt_priv, 1 );

	tnt_writeb( tnt_priv, STOP, CMDR );
	udelay(1);

	nec7210_set_reg_bits( nec_priv, IMR1, 0xff, imr1_bits );
	nec7210_set_reg_bits( nec_priv, IMR2, 0xff, imr2_bits );

	if( retval < 0 )
		return retval;
	/* XXX need to read counters to see how many bytes were written for
	 * cases when transfer is aborted */
	return length;
}

ssize_t tnt4882_accel_write( gpib_board_t *board, uint8_t *buffer, size_t length, int send_eoi )
{
	return generic_write( board, buffer, length, send_eoi, 0 );
}

ssize_t tnt4882_command( gpib_board_t *board, uint8_t *buffer, size_t length )
{
	return generic_write( board, buffer, length, 0, 1 );
}











