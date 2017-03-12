#include <stdio.h>
#include <debug.h>

int main( int argc, char **argv )
{
	if(argc<3)
	{
		fprintf( stderr, "usage : %s socketlisten_port Serialdev_path.\n", argv[0] );
		exit( EXIT_FAILURE );
	}
	MSG("\n");
	MSG("         ****************************************************\n");
	MSG("         ********         SmartHome Gateway          ********\n");
	MSG("         ****************************************************\n");
	MSG("\n");