/***************************************************************************
                                gpib_types.h
                             -------------------

    copyright            : (C) 2002 by Frank Mori Hess
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

#ifndef _GPIB_TYPES_H
#define _GPIB_TYPES_H

#ifdef __KERNEL__
/* gpib_interface_t defines the interface
 * between the board-specific details dealt with in the drivers
 * and generic interface provided by gpib-common.
 * This really should be in a different header file.
 */
#include <linux/wait.h>
#include <linux/timer.h>
#include <asm/semaphore.h>

typedef struct gpib_interface_struct gpib_interface_t;
typedef struct gpib_board_struct gpib_board_t;

struct gpib_interface_struct
{
	// list_head so we can make a linked list of drivers
	struct list_head list;
	// name of board
	char *name;
	/* attach() initializes board and allocates resources */
	int (*attach)(gpib_board_t *board);
	/* detach() shuts down board and frees resources */
	void (*detach)(gpib_board_t *board);
	/* read() should read at most 'length' bytes from the bus into
	 * 'buffer'.  It should return when it fills the buffer or
	 * encounters an END (EOI and or EOS if appropriate).  It should set 'end'
	 * to be nonzero if the read was terminated by an END, otherwise 'end'
	 * should be zero.
	 * Ultimately, this will be changed into or replaced by an asynchronous
	 * read.  Positive return value is number of bytes read, negative
	 * return indicates error.
	 */
	ssize_t (*read)(gpib_board_t *board, uint8_t *buffer, size_t length, int *end);
	/* write() should write 'length' bytes from buffer to the bus.
	 * If the boolean value send_eoi is nonzero, then EOI should
	 * be sent along with the last byte.  Returns number of bytes
	 * written or negative value on error.
	 */
	ssize_t (*write)(gpib_board_t *board, uint8_t *buffer, size_t length, int send_eoi);
	/* command() writes the command bytes in 'buffer' to the bus
	 * Returns number of bytes written or negative value on error.
	 */
	ssize_t (*command)(gpib_board_t *board, uint8_t *buffer, size_t length);
	/* Take control (assert ATN).  If 'asyncronous' is nonzero, take
	 * control asyncronously (assert ATN immediately without waiting
	 * for other processes to complete first).  Should not return
	 * until board becomes controller in charge.  Returns zero no success,
	 * nonzero on error.
	 */
	int (*take_control)(gpib_board_t *board, int asyncronous);
	/* De-assert ATN.  Returns zero on success, nonzer on error.
	 */
	int (*go_to_standby)(gpib_board_t *board);
	/* Asserts or de-asserts 'interface clear' (IFC) depending on
	 * boolean value of 'assert'
	 */
	void (*interface_clear)(gpib_board_t *board, int assert);
	/* Sends remote enable command if 'enable' is nonzero, disables remote mode
	 * if 'enable' is zero
	 */
	void (*remote_enable)(gpib_board_t *board, int enable);
	/* enable END for reads, when byte 'eos' is received.  If
	 * 'compare_8_bits' is nonzero, then all 8 bits are compared
	 * with the eos bytes.  Otherwise only the 7 least significant
	 * bits are compared. */
	void (*enable_eos)(gpib_board_t *board, uint8_t eos, int compare_8_bits);
	/* disable END on eos byte (END on EOI only)*/
	void (*disable_eos)(gpib_board_t *board);
	/* configure parallel poll */
	void (*parallel_poll_response)( gpib_board_t *board, uint8_t configuration );
	/* conduct parallel poll */
	int (*parallel_poll)(gpib_board_t *board, uint8_t *result);
	/* Returns current status of the bus lines.  Should be set to
	 * NULL if your board does not have the ability to query the
	 * state of the bus lines. */
	int ( *line_status )( const gpib_board_t *board );
	/* updates and returns the board's current status.
	 * The meaning of the bits are specified in gpib_user.h
	 * in the IBSTA section.  The driver does not need to
	 * worry about setting the CMPL, END, TIMO, or ERR bits.
	 */
	unsigned int (*update_status)(gpib_board_t *board);
	/* Sets primary address 0-30 for gpib interface card.
	 */
	void (*primary_address)(gpib_board_t *board, unsigned int address);
	/* Sets and enables, or disables secondary address 0-30
	 * for gpib interface card.
	 */
	void (*secondary_address)(gpib_board_t *board, unsigned int address,
	int enable);
	/* Sets the byte the board should send in response to a serial poll.
	 * Function should also request service if appropriate.
	 */
	void (*serial_poll_response)(gpib_board_t *board, uint8_t status);
	/* returns the byte the board will send in response to a serial poll.
	 */
	uint8_t ( *serial_poll_status )( gpib_board_t *board );
	/* Pointer to module whose use count we should increment when this
	 * interface is in use */
	struct module *provider_module;
};

/* One gpib_board_t is allocated for each physical board in the computer.
 * It provides storage for variables local to each board, and interface
 * functions for performing operations on the board */
struct gpib_board_struct
{
	/* functions used by this board */
	gpib_interface_t *interface;
	/* buffer used to store read/write data for this board */
	uint8_t *buffer;
	/* length of buffer */
	unsigned int buffer_length;
	/* Used to hold the board's current status (see update_status() above)
	 */
	volatile unsigned int status;
	/* Driver should only sleep on this wait queue.  It is special in that the
	 * core will wake this queue and set the TIMO bit in 'status' when the
	 * watchdog timer times out.
	 */
	wait_queue_head_t wait;
	/* Lock that only allows one process to access this board at a time */
	struct semaphore mutex;
	/* pid of last process to lock the board mutex */
	pid_t locking_pid;
	/* Lock that prevents more than one process from actively autopolling
	 * (we only need one autopoller) */
	struct semaphore autopoll_mutex;
	/* Spin lock for dealing with races with the interrupt handler */
	spinlock_t spinlock;
	/* Watchdog timer to enable timeouts */
	struct timer_list timer;
	/* IO base address to use for non-pnp cards (set by core, driver should make local copy) */
	unsigned long ibbase;
	/* IRQ to use for non-pnp cards (set by core, driver should make local copy) */
	unsigned int ibirq;
	/* dma channel to use for non-pnp cards (set by core, driver should make local copy) */
	unsigned int ibdma;
	/* 'private_data' can be used as seen fit by the driver to
	 * store additional variables for this board */
	void *private_data;
	/* Number of open file descriptors for this board */
	unsigned int open_count;
	/* list of open devices connected to this board */
	struct list_head device_list;
	// primary address
	unsigned int pad;
	// secondary address
	int sad;
	// timeout for io operations, in microseconds
	unsigned int usec_timeout;
	// board's parallel poll configuration byte
	uint8_t parallel_poll_configuration;
	/* Count that keeps track of whether board is up and running or not */
	unsigned int online;
	// number of processes trying to autopoll
	int autopollers;
	/* Flag that indicates whether board is system controller of the bus */
	unsigned master : 1;
	/* Flag board has been opened for exclusive access */
	unsigned exclusive : 1;
	// error dong autopoll
	unsigned stuck_srq : 1;
};

/* Each board has a list of gpib_device_t to keep track of all open devices
 * on the bus, so we know what address to poll when we get a service request */
typedef struct
{
	// list_head so we can make a linked list of devices
	struct list_head list;
	unsigned int pad;	// primary gpib address
	int sad;	// secondary gpib address (negative means disabled)
	// stores serial poll bytes for this device
	struct list_head status_bytes;
	unsigned int num_status_bytes;
	// number of times this address is opened
	unsigned int reference_count;
	// flags loss of status byte error due to limit on size of queue
	unsigned dropped_byte : 1;
} gpib_device_t;

typedef struct
{
	struct list_head list;
	uint8_t poll_byte;
} status_byte_t;

void init_gpib_device( gpib_device_t *device );

typedef struct
{
	struct list_head device_list;
	unsigned int online_count;
	unsigned holding_mutex : 1;
} gpib_file_private_t;

#endif	// __KERNEL__

#endif	// _GPIB_TYPES_H
