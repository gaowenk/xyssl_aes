/*
 *  AES-256 file encryption program
 *
 *  Copyright (C) 2006-2007  Christophe Devine
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  gaowenk change input/out format from files to strings  2017-08-15
 *  304702903@qq.com
 */

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#if defined(WIN32)
#include <windows.h>
#include <io.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "xyssl/aes.h"
#include "xyssl/sha2.h"

#define MODE_ENCRYPT    0
#define MODE_DECRYPT    1

#define MB_MAX_AES_LENGTH 128

unsigned int mb_aes_vals_count = 0;
unsigned char mb_aes_vals[MB_MAX_AES_LENGTH];
int mb_string2int (char *str);

#define USAGE   \
    "\n  aescrypt2 <mode> <input_string> <key>\n" \
    "\n       <mode>: 0 = encrypt, 1 = decrypt\n" \
    "\n  example1: aescrypt2str 0 china_beijing  hex:E76B2413958B00E193\n" \
    "\n            b9beaa2167a03274ad8dff05a15275bd" \
	"17fa0c903c7314fd7f8be51bb81038eb" \
    "03fa41bd45a00c38b76b4930fbf253634b74eb66d0a172cdddfa60cee3273dd9\n" \
    "\n  example2: aescrypt2str 1 b9beaa2167a03274ad8dff05a15275bd17fa0c903c7314fd7" \
    "f8be51bb81038eb03fa41bd45a00c38b76b4930fbf253634b74eb66d0a172cdddfa60cee3273dd9 hex:E76B2413958B00E193\n" \
    "\n            china-beijing" \
    "\n"

int main( int argc, char *argv[] )
{
    int ret = 1, i, n; int kk = 0;
    int keylen, mode, lastn;
    FILE *fkey, *fin, *fout;

    char *p;
    unsigned char IV[16];
    unsigned char key[512];
    unsigned char digest[32];
    unsigned char buffer[1024];

    aes_context aes_ctx;
    sha2_context sha_ctx;

#if defined(WIN32)
       LARGE_INTEGER li_size;
    __int64 filesize, offset;
#else
      off_t filesize, offset;
#endif

    /*
     * Parse the command-line arguments.
     */
    if( argc != 4 )
    {
        printf( USAGE );

#if defined(WIN32)
        printf( "\n  Press Enter to exit this program.\n" );
        fflush( stdout ); getchar();
#endif

        goto exit;
    }

    mode = atoi( argv[1] );

    if( mode != MODE_ENCRYPT && mode != MODE_DECRYPT )
    {
        fprintf( stderr, "invalide operation mode\n" );
        goto exit;
    }

    /*
     * Read the secret key and clean the command line.
     */
    {
        if( memcmp( argv[3], "hex:", 4 ) == 0 )
        {
            p = &argv[3][4];
            keylen = 0;

            while( sscanf( p, "%02X", &n ) > 0 &&
                   keylen < (int) sizeof( key ) )
            {
                key[keylen++] = (unsigned char) n;
                p += 2;
            }
        }
        else
        {
            keylen = strlen( argv[4] );

            if( keylen > (int) sizeof( key ) )
                keylen = (int) sizeof( key );

            memcpy( key, argv[4], keylen );
        }
    }

    memset( argv[3], 0, strlen( argv[3] ) );
    filesize = strlen(argv[2]);

    if( mode == MODE_ENCRYPT )
    {
        /*
         * Generate the initialization vector as:
         * IV = SHA-256( filesize || filename )[0..15]
         */
        for( i = 0; i < 8; i++ )
            buffer[i] = (unsigned char)( filesize >> ( i << 3 ) );

        p = argv[2];

        sha2_starts( &sha_ctx, 0 );
        sha2_update( &sha_ctx, buffer, 8 );
        sha2_update( &sha_ctx, (unsigned char *) p, strlen( p ) );
        sha2_finish( &sha_ctx, digest );

        memcpy( IV, digest, 16 );

        /*
         * The last four bits in the IV are actually used
         * to store the file size modulo the AES block size.
         */
        lastn = (int)( filesize & 0x0F );

        IV[15] = (unsigned char)
            ( ( IV[15] & 0xF0 ) | lastn );

        /*
         * Append the IV at the beginning of the output.
         */
        for(kk = 0; kk < 16; kk++) {printf("%02x", IV[kk]);}; printf("\n");

        /*
         * Hash the IV and the secret key together 8192 times
         * using the result to setup the AES context and HMAC.
         */
        memset( digest, 0,  32 );
        memcpy( digest, IV, 16 );

        for( i = 0; i < 8192; i++ )
        {
            sha2_starts( &sha_ctx, 0 );
            sha2_update( &sha_ctx, digest, 32 );
            sha2_update( &sha_ctx, key, keylen );
            sha2_finish( &sha_ctx, digest );
        }

        memset( key, 0, sizeof( key ) );
          aes_setkey_enc( &aes_ctx, digest, 256 );
        sha2_hmac_starts( &sha_ctx, digest, 32, 0 );

        /*
         * Encrypt and write the ciphertext.
         */
        for( offset = 0; offset < filesize; offset += 16 )
        {
            n = ( filesize - offset > 16 ) ? 16 : (int)
                ( filesize - offset );

            memcpy(buffer, argv[2], strlen(argv[2]) + 1);

            for( i = 0; i < 16; i++ )
                buffer[i] = (unsigned char)( buffer[i] ^ IV[i] );

            aes_crypt_ecb( &aes_ctx, AES_ENCRYPT, buffer, buffer );
            sha2_hmac_update( &sha_ctx, buffer, 16 );

            for(kk = 0; kk < 16; kk++) {printf("%02x", buffer[kk]);}; printf("\n");

            memcpy( IV, buffer, 16 );
        }

        /*
         * Finally write the HMAC.
         */
        sha2_hmac_finish( &sha_ctx, digest );

        for(kk = 0; kk < 32; kk++) {printf("%02x", digest[kk]);}; printf("\n");	
    }

    if( mode == MODE_DECRYPT )
    {
        unsigned char tmp[16];
        mb_string2int(argv[2]);
		filesize = mb_aes_vals_count;

        /*
         *  The encrypted file must be structured as follows:
         *
         *        00 .. 15              Initialization Vector
         *        16 .. 31              AES Encrypted Block #1
         *           ..
         *      N*16 .. (N+1)*16 - 1    AES Encrypted Block #N
         *  (N+1)*16 .. (N+1)*16 + 32   HMAC-SHA-256(ciphertext)
         */
        if( mb_aes_vals_count < 48 )
        {
            fprintf( stderr, "File too short to be encrypted.\n" );
            goto exit;
        }

        if( ( filesize & 0x0F ) != 0 )
        {
            fprintf( stderr, "File size not a multiple of 16.\n" );
            goto exit;
        }

        /*
         * Substract the IV + HMAC length.
         */
        filesize -= ( 16 + 32 );

        /*
         * Read the IV and original filesize modulo 16.
         */
        memcpy(buffer, mb_aes_vals, MB_MAX_AES_LENGTH);

        memcpy( IV, buffer, 16 );
        lastn = IV[15] & 0x0F;

        /*
         * Hash the IV and the secret key together 8192 times
         * using the result to setup the AES context and HMAC.
         */
        memset( digest, 0,  32 );
        memcpy( digest, IV, 16 );

        for( i = 0; i < 8192; i++ )
        {
            sha2_starts( &sha_ctx, 0 );
            sha2_update( &sha_ctx, digest, 32 );
            sha2_update( &sha_ctx, key, keylen );
            sha2_finish( &sha_ctx, digest );
        }

        memset( key, 0, sizeof( key ) );
          aes_setkey_dec( &aes_ctx, digest, 256 );
        sha2_hmac_starts( &sha_ctx, digest, 32, 0 );

        /*
         * Decrypt and write the plaintext.
         */
        for( offset = 0; offset < filesize; offset += 16 )
        {
            // printf("offset=%d filesize=%d offset=%d\n", offset, filesize, offset);
            memcpy( buffer, mb_aes_vals + 16 + offset, 16 );            

            memcpy( tmp, buffer, 16 );
 
            sha2_hmac_update( &sha_ctx, buffer, 16 );
            aes_crypt_ecb( &aes_ctx, AES_DECRYPT, buffer, buffer );
   
            for( i = 0; i < 16; i++ )
                buffer[i] = (unsigned char)( buffer[i] ^ IV[i] );

            memcpy( IV, tmp, 16 );

            n = ( lastn > 0 && offset == filesize - 16 )
                ? lastn : 16;

            printf("%s\n", buffer);
        }
    }

    ret = 0;

exit:

    memset( buffer, 0, sizeof( buffer ) );
    memset( digest, 0, sizeof( digest ) );

    memset( &aes_ctx, 0, sizeof(  aes_context ) );
    memset( &sha_ctx, 0, sizeof( sha2_context ) );

    return( ret );
}

int mb_string2int (char *str) {
	int len = 0;
	int ii = 0;
	unsigned char val = 0;
	unsigned char val0 = 0; 
	unsigned char val1 = 0;

	len = strlen(str);
	for (ii = 0; ii < len; ii += 2) {
		if(str[ii] <= '9'){ 
		   val0 = (str[ii] - '0');
		} else {
		   val0 = (str[ii] - 'W');
		}

		if(str[ii + 1] <= '9'){ 
			   val1 = str[ii + 1]  - '0';
		} else {
		   val1 = str[ii + 1]  - 'W';
		}

		val = val0 * 16 + val1;
		// printf("%x ", val0 * 16 + val1);

		mb_aes_vals_count++;
		mb_aes_vals[ii / 2] = val;
	}

	// printf("\n");
	return val;
}

