
#include "ib_internal.h"
#include <ibP.h>

int ibrpp( int ud, char *ppr )
{
	ibConf_t *conf;
	ibBoard_t *board;
	int retval;
	uint8_t poll_byte;

	conf = enter_library( ud, 1 );
	if( conf == NULL )
		return exit_library( ud, 1 );

	board = interfaceBoard( conf );

	set_timeout( board, conf->usec_timeout );

	retval = ioctl( board->fileno, IBRPP, &poll_byte );
	if( retval < 0 )
	{
		return exit_library( ud, 1 );
	}

	*ppr = poll_byte;

	return exit_library( ud, 0 );
}
