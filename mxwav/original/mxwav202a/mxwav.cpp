// MXDRVg WAV file generator 「mxwav」
// Copyright (C) 2000 GORRY.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <mbstring.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>
#include <windows.h>
#define __MXDRV_LOADMODULE
#include "mxdrv.h"

//

int SampleRate = 44100;
int MDXBuf = 128;
int PDXBuf = 1024;
int PCM8 = 1;

int (*MXDRV_Start)( int, int, int, int, int, int, int );
void (*MXDRV_End)( void );
int (*MXDRV_GetPCM)( void *, int );
void (*MXDRV_Play)( void *mdx, DWORD mdxsize, void *pdx, DWORD pdxsize );
void (*MXDRV)( X68REG *reg );
void volatile *(*MXDRV_GetWork)( int i );

MXWORK_CH *FM;
MXWORK_CH *PCM;
MXWORK_GLOBAL *G;
MXWORK_OPM *OPM;

TCHAR MDXTitle[256];

int MDXTerminate;
int LoopLimit = 2;
int LoopCount;
int PostWait = 200;
char *MDXFileName;
char OutputWAVFile[FILENAME_MAX];
int FadeoutStart;
int Bench=FALSE;
DWORD MaskChannel;

void CALLBACK MXCALLBACK_OPMINTfunc(
	void
);

//

int LoadMXDRV(
	void
) {
	HINSTANCE hinstMXDRV;

	hinstMXDRV = LoadLibrary( "mxdrv.dll" );
	if ( hinstMXDRV == NULL ) {
		fprintf( stderr, "mxdrv.dllの読み込みに失敗しました。\n" );
		return (!0);
	}

	MXDRV_Start   = (int (*)(int, int, int, int, int, int, int))GetProcAddress( hinstMXDRV, "MXDRV_Start" );
	MXDRV_End     = (void (*)(void))GetProcAddress( hinstMXDRV, "MXDRV_End" );
	MXDRV_GetPCM  = (int (*)(void *, int))GetProcAddress( hinstMXDRV, "MXDRV_GetPCM" );
	MXDRV_Play    = (void (*)(void *, DWORD, void *, DWORD))GetProcAddress( hinstMXDRV, "MXDRV_Play" );
	MXDRV         = (void (*)(X68REG *))GetProcAddress( hinstMXDRV, "MXDRV" );
	MXDRV_GetWork = (void volatile *(*)(int))GetProcAddress( hinstMXDRV, "MXDRV_GetWork" );

	if ( MXDRV_Start( SampleRate, 0, 500, 5, MDXBuf*1024, PDXBuf*1024, 0 ) ) {
		fprintf( stderr, "mxdrv.dllの初期化に失敗しました。\n" );
		return (!0);
	}

	{
		MXCALLBACK_OPMINTFUNC **p;
		p = (MXCALLBACK_OPMINTFUNC **)MXDRV_GetWork( MXDRV_CALLBACK_OPMINT );
		*(p) = &MXCALLBACK_OPMINTfunc;
	}

	FM = (MXWORK_CH *)MXDRV_GetWork( MXDRV_WORK_FM );
	PCM = (MXWORK_CH *)MXDRV_GetWork( MXDRV_WORK_PCM );
	G = (MXWORK_GLOBAL *)MXDRV_GetWork( MXDRV_WORK_GLOBAL );
	OPM = (MXWORK_OPM *)MXDRV_GetWork( MXDRV_WORK_OPM );

	return (0);
}

//

static void AddDirMark(
	TCHAR *p
) {
	int len;

	len = lstrlen( p );
	if ( _ismbslead( (const unsigned char *)p, (const unsigned char *)(p+len) ) ) {
		if ( p[len] == '\\' ) return;
	}
	p[len] = '\\';
	p[len+1] = '\0';
}

//

static void ExtractPath(
	TCHAR *p
) {
	TCHAR c;
	TCHAR *p2;

	do {
		c = *(p);
		if ( c == '\0' ) break;
		if ( _ismbblead(c) ) {
			p = (TCHAR *)_mbsinc( (const unsigned char *)p );
		} else {
			if ( c == '\\' ) {
				p2 = p;
			}
			p++;
		}
	} while (!0);
	p2[1] = '\0';
}

//

static int PlayMDX(
	TCHAR *mdxname
) {
	TCHAR mdxfilename[FILENAME_MAX+1];
	TCHAR pdxfilename[FILENAME_MAX+1];
	TCHAR pdxfilename2[FILENAME_MAX+1];
	TCHAR pdxpath[FILENAME_MAX+1];
	TCHAR mdxpath[FILENAME_MAX+1];
	BYTE *mdxbuf;
	BYTE *pdxbuf;
	FILE *fin;
	BYTE *p;
	BYTE *p2;
	int mdxsize;
	int pdxsize;
	DWORD mdxbodyptr;
	DWORD pdxbodyptr;
	int havepdx;
	int pdxnamelen;
	TCHAR *dummy;
	UBYTE *pcm8ptr;
	int mdxbufsize;
	int pdxbufsize;
	BYTE c;
	char *env;

	// PCM8の使用を決定
	pcm8ptr = (UBYTE *)MXDRV_GetWork(MXDRV_WORK_PCM8);
	*(pcm8ptr) = ( PCM8 ? 1 : 0 );

	// PDXパス名を生成
	env = getenv( "mxp" );
	if ( env == NULL ) {
		env = "";
	}
	lstrcpy( pdxpath, env );
	{
		TCHAR *p;
		p = &pdxpath[lstrlen(pdxpath)];
		if ( p == pdxpath ) {
			*(p++) = '.';
			*(p) = '\0';
		}
		AddDirMark( pdxpath );
	}

	// MDXパス名を生成
	GetFullPathName( mdxname, sizeof(mdxfilename), mdxfilename, &dummy );
	lstrcpy( mdxpath, mdxfilename );
	ExtractPath( mdxpath );

	// メモリ確保
	mdxbufsize = MDXBuf*1024;
	pdxbufsize = PDXBuf*1024;
	mdxbuf = (BYTE *)malloc( mdxbufsize );
	pdxbuf = (BYTE *)malloc( pdxbufsize );
	if ( (mdxbuf == NULL) || (pdxbuf == NULL) ) goto err_malloc;

	// MDXを読み込む
	fin = fopen( mdxfilename, "rb" );
	if ( fin == NULL ) goto err_readmdx;
	fseek( fin, 0, SEEK_END );
	if ( ftell( fin ) > mdxbufsize ) goto err_mdxbuf;
	fseek( fin, 0, SEEK_SET);
	mdxsize = fread( mdxbuf+8, 1, mdxbufsize-8, fin );
	fclose( fin );

	// タイトルをスキップ
	p = mdxbuf+8;
	while ( mdxsize-- ) {
		c = *(p++);
		if ( c == 0x0d ) break;
		if ( c < 0x20 ) {
			if ( c != 0x1b ) goto err_brokenmdx;
		}
	}
	if ( mdxsize <= 0 ) goto err_brokenmdx;
	p--;
	*(p++) = 0x00;
	if ( ((DWORD)(p))&0x01 ) {
		*(p++) = 0x00;
		mdxsize--;
		if ( mdxsize <= 0 ) goto err_brokenmdx;
	}

	p2 = p;

	// PDXを読み込む
	havepdx = 0;
	while ( mdxsize--, *(p2++) != 0x1a ) {
		if ( mdxsize <= 0 ) goto err_brokenmdx;
	}
	if ( *(p2) ) {
		havepdx = !0;
		wsprintf( pdxfilename, "%s%s", mdxpath, p2 );
		wsprintf( pdxfilename2, "%s%s", mdxpath, p2 );
		fin = fopen( pdxfilename, "rb" );
		if ( fin == NULL ) {
			wsprintf( pdxfilename, "%s%s.pdx", mdxpath, p2 );
			wsprintf( pdxfilename2, "%s%s.pdx", mdxpath, p2 );
			fin = fopen( pdxfilename, "rb" );
			if ( fin == NULL ) {
				wsprintf( pdxfilename, "%s%s", pdxpath, p2 );
				wsprintf( pdxfilename2, "%s", p2 );
				fin = fopen( pdxfilename, "rb" );
				if ( fin == NULL ) {
					wsprintf( pdxfilename, "%s%s.pdx", pdxpath, p2 );
					wsprintf( pdxfilename2, "%s.pdx", p2 );
					fin = fopen( pdxfilename, "rb" );
					if ( fin == NULL ) {
						fprintf( stderr, "PDXファイル(%s)が見つかりません。\n", p2 );
						havepdx = 0;
					}
				}
			}
		}
		if ( havepdx ) {
			fseek( fin, 0, SEEK_END );
			if ( ftell( fin ) > pdxbufsize ) goto err_pdxbuf;
			fseek( fin, 0, SEEK_SET );
			pdxnamelen = lstrlen( pdxfilename2 )+1;
			pdxbodyptr = (8+pdxnamelen+1)&(-2);
			lstrcpy( (TCHAR *)(pdxbuf+8), (TCHAR *)(pdxfilename2) );
			pdxsize = fread( pdxbuf+pdxbodyptr, 1, pdxbufsize-pdxbodyptr, fin );
			fclose( fin );
			pdxbuf[0] = 0x00;
			pdxbuf[1] = 0x00;
			pdxbuf[2] = 0x00;
			pdxbuf[3] = 0x00;
			pdxbuf[4] = pdxbodyptr>>8;
			pdxbuf[5] = pdxbodyptr;
			pdxbuf[6] = pdxnamelen>>8;
			pdxbuf[7] = pdxnamelen;
			pdxsize += pdxbodyptr;
		}
	}

	// MDXをMXDRVへ渡せるよう加工する
	while ( mdxsize--, *(p2++) ) {
		if ( mdxsize <= 0 ) goto err_brokenmdx;
	}
	mdxbodyptr = p - &mdxbuf[0];
	while ( mdxsize-- ) {
		*(p++) = *(p2++);
	}
	mdxsize = p - &mdxbuf[0];
	mdxbuf[0] = 0x00;
	mdxbuf[1] = 0x00;
	mdxbuf[2] = (havepdx ? 0 : 0xff);
	mdxbuf[3] = (havepdx ? 0 : 0xff);
	mdxbuf[4] = mdxbodyptr>>8;
	mdxbuf[5] = mdxbodyptr;
	mdxbuf[6] = 0x00;
	mdxbuf[7] = 0x08;
	lstrcpy( MDXTitle, (TCHAR *)&mdxbuf[8] );

	// MXDRV呼び出し
	MXDRV_Stop();
	if ( havepdx ) {
		MXDRV_Play( mdxbuf, mdxsize, pdxbuf, pdxsize );
	} else {
		MXDRV_Play( mdxbuf, mdxsize, NULL, 0 );
	}

	// チャンネルマスク
	G->L001e1c = (UWORD)MaskChannel;

	// メモリ解放
	free( mdxbuf );
	free( pdxbuf );

	return (0);

  err_malloc:;
	fprintf( stderr, "メモリ確保に失敗しました。\n" );
	return (!0);

  err_readmdx:;
	fprintf( stderr, "MDXファイル(%s)の読み込みに失敗しました。\n", mdxfilename );
	return (!0);

  err_brokenmdx:;
	fprintf( stderr, "ファイル(%s)はMDXでないか、壊れています。\n", mdxfilename );
	return (!0);

  err_mdxbuf:;
	fclose( fin );
	fprintf( stderr, "ファイル(%s)を読むMDXバッファが足りません。\n", mdxfilename );
	return (!0);

  err_pdxbuf:;
	fclose( fin );
	fprintf( stderr, "ファイル(%s)を読むPDXバッファが足りません。\n", pdxfilename );
	return (!0);

}

//

void CALLBACK MXCALLBACK_OPMINTfunc(
	void
) {
	if ( G->L001e13 != 0 ) {
		MDXTerminate = TRUE;
	}
	if ( G->L002246 == 65535 ) {
		MDXTerminate = TRUE;
	} else {
		LoopCount = G->L002246;
		if ( !FadeoutStart ) {
			if ( LoopCount >= LoopLimit ) {
				FadeoutStart = TRUE;
				MXDRV_Fadeout();
			}
		}
	}
}

//

void PrintHelp( void )
{
	fprintf( stderr,
   "mxwav: MXDRV music recorder: V2.02a: Copyright (C) 2000-2013 GORRY.\n"
   " MDXファイルによる演奏をWAVEファイルへ録音します。\n"
   " Usage: mxwav [Option] [MDXファイル]\n"
   " PDX:   PDX検索パスは環境変数\"mxp\"で指定します。\n"
   " Option:"
   "-Ln\tn回ループでフェードアウトします(def=2)。\n"
   "\t-Sn\tサンプリングレートを22050または44100で指定します(def=44100)。\n"
   "\t-Pn\tPDXバッファをKB単位で指定します(def=1024)。\n"
   "\t-Wn\t演奏終了後の待ち時間をms単位で指定します(def=2000)。\n"
   "\t-U\tPCM8を使用しません。\n"
   "\t-B\tベンチマークモード（WAVEファイルを生成しません）。\n"
   "\t-Ofile\t出力WAVEファイルを指定します(def=MDXファイルの拡張子をWAV化)。\n"
   "\t-Mch\tチャンネルをマスクします(ch=\"12345678\"(FM), \"PQRSTUVW\"(PCM))。\n"
	);
}

//

void AnalyzeArg( int argc, char *argv[] )
{
	int i;
	int a;

	for ( i=1; i<argc; i++ ) {
		switch ( argv[i][0] ) {
		  case '-':
			switch ( argv[i][1] ) {
			  case 'L':
			  case 'l':
				a = atoi( &argv[i][2] );
				if ( (a < 1) || (a > 255) ) {
					fprintf( stderr, "-Lの範囲は1～255です。\n" );
					exit(EXIT_FAILURE);
				}
				LoopLimit = a;
				break;
			  case 'S':
			  case 's':
				a = atoi( &argv[i][2] );
				if ( (a != 22050) && (a != 44100) && (a != 48000) ) {
					fprintf( stderr, "-Sは22050, 44100, 48000が指定できます。\n" );
					exit(EXIT_FAILURE);
				}
				SampleRate = a;
				break;
			  case 'P':
			  case 'p':
				a = atoi( &argv[i][2] );
				if ( a < 1 ) {
					fprintf( stderr, "-Pの範囲は1以上です。\n" );
					exit(EXIT_FAILURE);
				}
				PDXBuf = a;
				break;
			  case 'W':
			  case 'w':
				a = atoi( &argv[i][2] );
				if ( a < 0 ) {
					fprintf( stderr, "-Wの範囲は0以上です。\n" );
					exit(EXIT_FAILURE);
				}
				PostWait = a/10;
				break;
			  case 'U':
			  case 'u':
				PCM8 = 0;
				break;
			  case 'B':
			  case 'b':
				Bench = TRUE;
				break;
			  case 'O':
			  case 'o':
				lstrcpy( OutputWAVFile, &argv[i][2] );
				break;
			  case 'M':
			  case 'm':
				{
					char *p = &argv[i][2];
					while (*(p)) {
						int ch;
						int c = toupper(*(p++));
						switch (c) {
						  case '1':
						  case '2':
						  case '3':
						  case '4':
						  case '5':
						  case '6':
						  case '7':
						  case '8':
							ch = c-'1';
							MaskChannel |= (1<<ch);
							break;
						  case 'P':
						  case 'Q':
						  case 'R':
						  case 'S':
						  case 'T':
						  case 'U':
						  case 'V':
						  case 'W':
							ch = c-'P'+8;
							MaskChannel |= (1<<ch);
							break;
						}
					}
				}
				break;
			  case '?':
			  case 'h':
			  case 'H':
				PrintHelp();
				exit(EXIT_SUCCESS);
				break;
			  default:
				fprintf( stderr, "オプション(%s)が異常です。\n" );
				exit(EXIT_FAILURE);
			}
			break;
		  default:
			MDXFileName = argv[i];
			break;
		}
	}
}


//

#define WaveBufSize (44100/100) // 10ms分
#define N_WaveBuf 100
UBYTE WaveBuf[WaveBufSize*4*N_WaveBuf];

void onsigint( int )
{
	if ( MXDRV_End ) {
		MXDRV_Stop();
		MXDRV_End();
	}
	fprintf( stderr, "\nTerminated.\n" );
	exit(EXIT_FAILURE);
}

int main( int argc, char *argv[] )
{
	FILE *fout = NULL;
	char *p;
	char *p2;
	static BYTE wavheader[0x2c];
	unsigned long wavsize;
	unsigned long filesize;
	int postcount = 0;
	unsigned long ct;
	int ti;
	char *basename;
	char wavfullpath[FILENAME_MAX];
	BYTE *w;

	// CTRL-Cを塞ぐ
	signal( SIGINT, onsigint );

	// 引数解析
	AnalyzeArg( argc, argv );
	if ( MDXFileName == NULL ) {
		PrintHelp();
		exit(EXIT_FAILURE);
	}

	// WAVファイル名作成
	if ( !OutputWAVFile[0] ) {
		GetFullPathName( MDXFileName, sizeof(wavfullpath), wavfullpath, &basename );
		GetCurrentDirectory( sizeof(OutputWAVFile), OutputWAVFile );
		AddDirMark( OutputWAVFile );
		lstrcat( OutputWAVFile, basename );
		p = &OutputWAVFile[0];
		p2 = NULL;
		while ( *(p) ) {
			if ( *(p) == '.' ) {
				p2 = p;
			}
			p++;
		}
		if ( p2 == NULL ) {
			p2 = p;
		}
		lstrcpy( p2, ".wav" );
	}

	// 出力WAVEファイルを開く
	if ( !Bench ) {
		fout = fopen( OutputWAVFile, "wb" );
		if ( fout == NULL ) {
			fprintf( stderr, "出力ファイル(%s)が開けません。\n", OutputWAVFile );
			exit(EXIT_FAILURE);
		}
	}

	// 空のwaveヘッダを出力
	if ( fout ) {
		if ( sizeof(wavheader) != fwrite( wavheader, 1, sizeof( wavheader ), fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile );
			exit(EXIT_FAILURE);
		}
	}

	// 演奏開始
	if ( LoadMXDRV() != 0 ) {
		exit(EXIT_FAILURE);
	}
	if ( PlayMDX( MDXFileName ) != 0 ) {
		MXDRV_End();
		exit(EXIT_FAILURE);
	}
	fprintf( stderr, "TITLE: %s\n", MDXTitle );

	// 録音
	ct = 0;
	ti = 0;
	wavsize = 0;
	while ( !0 ) {
		if ( MDXTerminate ) {
			postcount++;
			if ( postcount >= PostWait ) break;
		}
		if ( ct == 0 ) {
			fprintf( stderr, "%02d:%02d / %ldKB / Loop %d\r", ti/60, ti%60, wavsize/1024, LoopCount+1 );
		}
		ct++;
		if ( ct >= N_WaveBuf ) {
			if ( fout ) {
				if ( ct*WaveBufSize*4 != fwrite( WaveBuf, 1, ct*WaveBufSize*4, fout ) ) {
					fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile );
					MXDRV_Stop();
					MXDRV_End();
					exit(EXIT_FAILURE);
				}
			}
			ct = 0;
			ti++;
		}
		MXDRV_GetPCM( &WaveBuf[ct*WaveBufSize*4], WaveBufSize );
		wavsize += WaveBufSize*4;
	}
	if ( fout ) {
		if ( ct*WaveBufSize*4 != fwrite( WaveBuf, 1, ct*WaveBufSize*4, fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile );
			MXDRV_Stop();
			MXDRV_End();
			exit(EXIT_FAILURE);
		}
	}
	fprintf( stderr, "%02d:%02d / %ldKB / 生成速度=%d%%              \n", ti/60, ti%60, wavsize/1024, (int)((double)ti*CLK_TCK/clock()*100) );

	// 演奏終了
	MXDRV_Stop();
	MXDRV_End();

	// WAVEヘッダ作成
	filesize = wavsize+sizeof(wavheader);
	w = &wavheader[0];
	/* 0x00 */ *(w++) = 'R';
	/* 0x01 */ *(w++) = 'I';
	/* 0x02 */ *(w++) = 'F';
	/* 0x03 */ *(w++) = 'F';
	/* 0x04 */ *(w++) = (BYTE)((filesize-8)>>0); // ファイルサイズ-4
	/* 0x05 */ *(w++) = (BYTE)((filesize-8)>>8);
	/* 0x06 */ *(w++) = (BYTE)((filesize-8)>>16);
	/* 0x07 */ *(w++) = (BYTE)((filesize-8)>>24);
	/* 0x08 */ *(w++) = 'W';
	/* 0x09 */ *(w++) = 'A';
	/* 0x0a */ *(w++) = 'V';
	/* 0x0b */ *(w++) = 'E';
	/* 0x0c */ *(w++) = 'f';
	/* 0x0d */ *(w++) = 'm';
	/* 0x0e */ *(w++) = 't';
	/* 0x0f */ *(w++) = ' ';
	/* 0x10 */ *(w++) = (BYTE)(0x10>>0);  // ヘッダサイズ
	/* 0x11 */ *(w++) = (BYTE)(0x10>>8);
	/* 0x12 */ *(w++) = (BYTE)(0x10>>16);
	/* 0x13 */ *(w++) = (BYTE)(0x10>>24);
	/* 0x14 */ *(w++) = (BYTE)(0x01>>0);  // 0x0001で固定
	/* 0x15 */ *(w++) = (BYTE)(0x01>>8);
	/* 0x16 */ *(w++) = (BYTE)(0x02>>0);  // 出力チャンネル数
	/* 0x17 */ *(w++) = (BYTE)(0x02>>8);
	/* 0x18 */ *(w++) = (BYTE)(SampleRate>>0);	// サンプリングレート
	/* 0x19 */ *(w++) = (BYTE)(SampleRate>>8);
	/* 0x1a */ *(w++) = (BYTE)(SampleRate>>16);
	/* 0x1b */ *(w++) = (BYTE)(SampleRate>>24);
	/* 0x1c */ *(w++) = (BYTE)((SampleRate*4)>>0);	// １秒あたりのバイト数
	/* 0x1d */ *(w++) = (BYTE)((SampleRate*4)>>8);
	/* 0x1e */ *(w++) = (BYTE)((SampleRate*4)>>16);
	/* 0x1f */ *(w++) = (BYTE)((SampleRate*4)>>24);
	/* 0x20 */ *(w++) = (BYTE)(4>>0);	// １サンプルのバイト数
	/* 0x21 */ *(w++) = (BYTE)(4>>8);
	/* 0x22 */ *(w++) = (BYTE)(0x10>>0);	// １データのビット数
	/* 0x23 */ *(w++) = (BYTE)(0x10>>8);
	/* 0x24 */ *(w++) = 'd';
	/* 0x25 */ *(w++) = 'a';
	/* 0x26 */ *(w++) = 't';
	/* 0x27 */ *(w++) = 'a';
	/* 0x28 */ *(w++) = (BYTE)(wavsize>>0);
	/* 0x29 */ *(w++) = (BYTE)(wavsize>>8);
	/* 0x2a */ *(w++) = (BYTE)(wavsize>>16);
	/* 0x2b */ *(w++) = (BYTE)(wavsize>>24);
	if ( fout ) {
		rewind( fout );
		if ( sizeof(wavheader) != fwrite( wavheader, 1, sizeof( wavheader ), fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile );
			exit(EXIT_FAILURE);
		}
	}

	// 出力WAVEファイルを閉じる
	if ( fout ) {
		if ( fclose( fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile );
			exit(EXIT_FAILURE);
		}
	}

	return ( EXIT_SUCCESS );
}

// [EOF]
