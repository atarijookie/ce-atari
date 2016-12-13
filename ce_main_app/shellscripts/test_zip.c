#include <stdio.h>
#include "../newscripts_zip.h"

int main(int argc, char * * argv)
{
	FILE * f;
	f = fopen("test.zip", "wb");
	if(!f) return 1;
	fwrite(newscripts_zip, 1, newscripts_zip_len, f);
	fclose(f);
	return 0;
}
