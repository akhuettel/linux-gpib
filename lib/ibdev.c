/***************************************************************************
                                 ibdev.c
                             -------------------
    begin                : Tues Feb 12 2002
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

#include "ib_internal.h"
#include <ibP.h>
#include <stdlib.h>

int config_parsed = 0;

int ibdev(int minor, int pad, int sad, int timo, int eot, int eos)
{
	char *envptr;
	int retval;
	int uDesc;
	ibConf_t conf;
	ibBoard_t *board;

	/* load config */

	if(config_parsed == 0)
	{
		envptr = getenv("IB_CONFIG");
		if(envptr)
			retval = ibParseConfigFile(envptr);
		else
			retval = ibParseConfigFile(DEFAULT_CONFIG_FILE);
		if(retval < 0)
		{
			ibsta |= ERR;
			ibPutErrlog(-1,"ibParseConfig");
			return -1;
		}
		config_parsed = 1;
	}

	conf.pad = pad;
	conf.sad = sad - sad_offset;                        /* device address                   */
	conf.init_string[0] = 0;               /* initialization string (optional) */
	conf.board = minor;                         /* board number                     */
	conf.eos = eos & 0xff;                           /* local eos modes                  */
	conf.eosflags = (eos >> 8) & 0xff;
	conf.tmo = timo;
	if(eot)
		conf.send_eoi = 1;
	else
		conf.send_eoi = 0;
	conf.flags = 0;
	// check if it is an interface board
	board = &ibBoard[minor];
	if(board->pad == conf.pad && board->sad == conf.sad)
	{
		conf.is_interface = 1;
		if(board->ifc)
			conf.flags |= CN_ISCNTL;
	}else
		conf.is_interface = 0;

	uDesc = ibGetDescriptor(conf);
	if(uDesc < 0)
	{
		fprintf(stderr, "ibdev failed to get descriptor\n");
		return -1;
	}

	if(ibBdChrConfig(uDesc) & ERR)
		return -1;

	if(ibonl(uDesc, 1) & ERR)
	{
		fprintf(stderr, "failed to bring device online\n");
		return -1;
	}

	if(ibsre(uDesc, 1) & ERR ) return -1;

	return uDesc;
}