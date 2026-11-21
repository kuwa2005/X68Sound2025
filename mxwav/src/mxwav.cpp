// MXDRVg WAV file generator 「mxwav」
// Copyright (C) 2000 GORRY.
//
// [port] Windows / Linux portable version (X68Sound2025)
//   - mxdrv.dll / X68Sound.dll are linked statically (no LoadLibrary)
//   - Builds as 32-bit or 64-bit (64-bit: MXDRVg buffers are placed in low
//     memory, see lowmem.h)
//   - Path handling for both '/' and '\\'; PDX lookup is case-insensitive on
//     case-sensitive file systems
//   - The MDX title (Shift_JIS) is converted to UTF-8 for display
//   - The WAV header uses the real output rate (X68SOUND_OUTPUT_RATE aware)
//   - -S accepts 96000 / 192000 (X68Sound2025 high-resolution output)
//   - PDX search path: environment variable "mxp" (original) or "PDX"
// Original source: original/mxwav202a/mxwav.cpp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <iconv.h>
#include <unistd.h>
#include <limits.h>
#include <strings.h>
#include "windows.h"	// compat/linux
#endif

#include "mxdrv/mxdrv.h"
#include "mxdrv/x68sound.h"
#include "lowmem.h"

//

int SampleRate = 44100;
int MDXBuf = 128;
int PDXBuf = 1024;
int PCM8 = 1;

MXWORK_CH *FM;
MXWORK_CH *PCM;
MXWORK_GLOBAL *G;
MXWORK_OPM *OPM;

static std::string MDXTitle;

volatile int MDXTerminate;
int LoopLimit = 2;
volatile int LoopCount;
int PostWait = 200;
char *MDXFileName;
static std::string OutputWAVFile;
volatile int FadeoutStart;
int Bench = FALSE;
DWORD MaskChannel;

void CALLBACK MXCALLBACK_OPMINTfunc(
	void
);

// ---------------------------------------------------------------------------
// [port] helpers
// ---------------------------------------------------------------------------

static bool IsSep(char c) {
	return c == '/' || c == '\\';
}

// Directory part of a path, including the trailing separator ("" if none).
// Shift_JIS lead bytes are skipped so that 0x5C in a trail byte is not taken
// as '\\' (same as the original _ismbblead handling).
static std::string DirName(const std::string &path) {
	size_t last = std::string::npos;
	for (size_t i = 0; i < path.size(); ++i) {
		unsigned char c = (unsigned char)path[i];
#ifdef _WIN32
		if (((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) && i + 1 < path.size()) { ++i; continue; }
#endif
		if (IsSep((char)c)) last = i;
	}
	return (last == std::string::npos) ? std::string() : path.substr(0, last + 1);
}

static std::string BaseName(const std::string &path) {
	return path.substr(DirName(path).size());
}

static std::string AddDirMark(const std::string &dir) {
	if (dir.empty()) return "./";
	if (IsSep(dir[dir.size() - 1])) return dir;
#ifdef _WIN32
	return dir + "\\";
#else
	return dir + "/";
#endif
}

// Shift_JIS (CP932) -> UTF-8 (for display / Linux file names)
static std::string SjisToUtf8(const std::string &s) {
	if (s.empty()) return s;
#ifdef _WIN32
	int wn = MultiByteToWideChar(932, 0, s.c_str(), (int)s.size(), NULL, 0);
	if (wn <= 0) return s;
	std::vector<wchar_t> w(wn);
	MultiByteToWideChar(932, 0, s.c_str(), (int)s.size(), w.data(), wn);
	int un = WideCharToMultiByte(CP_UTF8, 0, w.data(), wn, NULL, 0, NULL, NULL);
	std::string out(un, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), wn, &out[0], un, NULL, NULL);
	return out;
#else
	iconv_t cd = iconv_open("UTF-8", "CP932");
	if (cd == (iconv_t)-1) return s;
	std::string in(s), out(s.size() * 4 + 16, '\0');
	char *ip = &in[0], *op = &out[0];
	size_t il = in.size(), ol = out.size();
	while (il > 0) {
		if (iconv(cd, &ip, &il, &op, &ol) == (size_t)-1) {
			if (ol == 0) break;
			*op++ = '?'; --ol; ++ip; --il;   // skip invalid byte
		}
	}
	iconv_close(cd);
	out.resize(out.size() - ol);
	return out;
#endif
}

static FILE *OpenFile(const std::string &path) {
	return fopen(path.c_str(), "rb");
}

// Open dir+name. On case-sensitive file systems, also try a case-insensitive
// match and the UTF-8 form of a Shift_JIS name (X68000 file names are SJIS and
// usually upper case). Returns the opened file and the path actually used.
static FILE *OpenInDir(const std::string &dir, const std::string &name, std::string &opened) {
	opened = dir + name;
	FILE *f = OpenFile(opened);
	if (f) return f;
#ifndef _WIN32
	std::string names[2] = { name, SjisToUtf8(name) };
	std::string d = dir.empty() ? std::string("./") : dir;
	DIR *dp = opendir(d.c_str());
	if (!dp) return NULL;
	struct dirent *e;
	while ((e = readdir(dp)) != NULL) {
		for (int k = 0; k < 2; ++k) {
			if (strcasecmp(e->d_name, names[k].c_str()) == 0) {
				opened = dir + e->d_name;
				f = OpenFile(opened);
				if (f) { closedir(dp); return f; }
			}
		}
	}
	closedir(dp);
#endif
	return NULL;
}

static std::string FullPath(const char *p) {
#ifdef _WIN32
	char buf[MAX_PATH]; char *dummy;
	if (GetFullPathNameA(p, sizeof(buf), buf, &dummy)) return buf;
	return p;
#else
	char buf[PATH_MAX];
	if (realpath(p, buf)) return buf;
	return p;
#endif
}

static std::string CurrentDir() {
	char buf[4096];
#ifdef _WIN32
	if (GetCurrentDirectoryA(sizeof(buf), buf)) return buf;
#else
	if (getcwd(buf, sizeof(buf))) return buf;
#endif
	return ".";
}

//

static int PlayMDX(
	const char *mdxname
) {
	std::string pdxpath;
	std::string mdxfilename;
	std::string mdxpath;
	std::string pdxfilename;	// opened path
	std::string pdxfilename2;	// name stored in the PDX buffer
	BYTE *mdxbuf = NULL;
	BYTE *pdxbuf = NULL;
	FILE *fin;
	BYTE *p;
	BYTE *p2;
	long mdxsize;
	long pdxsize = 0;
	DWORD mdxbodyptr;
	DWORD pdxbodyptr;
	int havepdx;
	int pdxnamelen;
	UBYTE *pcm8ptr;
	int mdxbufsize;
	int pdxbufsize;
	BYTE c;
	const char *env;

	// PCM8の使用を決定
	pcm8ptr = (UBYTE *)MXDRV_GetWork(MXDRV_WORK_PCM8);
	*(pcm8ptr) = ( PCM8 ? 1 : 0 );

	// PDXパス名を生成
	env = getenv( "mxp" );
	if ( env == NULL ) env = getenv( "PDX" );	// [port] manual uses "PDX"
	if ( env == NULL ) env = "";
	pdxpath = AddDirMark( env[0] ? std::string(env) : std::string(".") );

	// MDXパス名を生成
	mdxfilename = FullPath( mdxname );
	mdxpath = DirName( mdxfilename );

	// メモリ確保 ([port] MXDRVg handles these addresses as 32-bit values)
	mdxbufsize = MDXBuf*1024;
	pdxbufsize = PDXBuf*1024;
	mdxbuf = (BYTE *)LowAlloc( mdxbufsize );
	pdxbuf = (BYTE *)LowAlloc( pdxbufsize );
	if ( (mdxbuf == NULL) || (pdxbuf == NULL) ) goto err_malloc;

	// MDXを読み込む
	fin = OpenFile( mdxfilename );
	if ( fin == NULL ) goto err_readmdx;
	fseek( fin, 0, SEEK_END );
	if ( ftell( fin ) > mdxbufsize-8 ) goto err_mdxbuf;
	fseek( fin, 0, SEEK_SET);
	mdxsize = (long)fread( mdxbuf+8, 1, mdxbufsize-8, fin );
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
	if ( ((uintptr_t)(p))&0x01 ) {
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
		std::string pdxname( (const char *)p2 );
		havepdx = !0;
		fin = NULL;
		{
			// 検索順: MDXと同じフォルダ(名前/名前.pdx) → PDXパス(名前/名前.pdx)
			struct { const std::string *dir; std::string name; bool storeFull; } cand[4] = {
				{ &mdxpath, pdxname, true }, { &mdxpath, pdxname + ".pdx", true },
				{ &pdxpath, pdxname, false }, { &pdxpath, pdxname + ".pdx", false },
			};
			for ( int i = 0; i < 4 && fin == NULL; ++i ) {
				fin = OpenInDir( *cand[i].dir, cand[i].name, pdxfilename );
				if ( fin ) pdxfilename2 = cand[i].storeFull ? (*cand[i].dir + cand[i].name) : cand[i].name;
			}
		}
		if ( fin == NULL ) {
			fprintf( stderr, "PDXファイル(%s)が見つかりません。\n", SjisToUtf8(pdxname).c_str() );
			havepdx = 0;
		}
		if ( havepdx ) {
			fseek( fin, 0, SEEK_END );
			long fsz = ftell( fin );
			fseek( fin, 0, SEEK_SET );
			pdxnamelen = (int)pdxfilename2.size()+1;
			pdxbodyptr = (8+pdxnamelen+1)&(-2);
			if ( fsz > (long)(pdxbufsize-pdxbodyptr) ) goto err_pdxbuf;
			memcpy( pdxbuf+8, pdxfilename2.c_str(), pdxnamelen );
			pdxsize = (long)fread( pdxbuf+pdxbodyptr, 1, pdxbufsize-pdxbodyptr, fin );
			fclose( fin );
			pdxbuf[0] = 0x00;
			pdxbuf[1] = 0x00;
			pdxbuf[2] = 0x00;
			pdxbuf[3] = 0x00;
			pdxbuf[4] = (BYTE)(pdxbodyptr>>8);
			pdxbuf[5] = (BYTE)pdxbodyptr;
			pdxbuf[6] = (BYTE)(pdxnamelen>>8);
			pdxbuf[7] = (BYTE)pdxnamelen;
			pdxsize += pdxbodyptr;
		}
	}

	// MDXをMXDRVへ渡せるよう加工する
	while ( mdxsize--, *(p2++) ) {
		if ( mdxsize <= 0 ) goto err_brokenmdx;
	}
	mdxbodyptr = (DWORD)(p - &mdxbuf[0]);
	while ( mdxsize-- > 0 ) {
		*(p++) = *(p2++);
	}
	mdxsize = (long)(p - &mdxbuf[0]);
	mdxbuf[0] = 0x00;
	mdxbuf[1] = 0x00;
	mdxbuf[2] = (havepdx ? 0 : 0xff);
	mdxbuf[3] = (havepdx ? 0 : 0xff);
	mdxbuf[4] = (BYTE)(mdxbodyptr>>8);
	mdxbuf[5] = (BYTE)mdxbodyptr;
	mdxbuf[6] = 0x00;
	mdxbuf[7] = 0x08;
	MDXTitle = (const char *)&mdxbuf[8];

	// MXDRV呼び出し
	MXDRV_Stop();
	if ( havepdx ) {
		MXDRV_Play( mdxbuf, (DWORD)mdxsize, pdxbuf, (DWORD)pdxsize );
	} else {
		MXDRV_Play( mdxbuf, (DWORD)mdxsize, NULL, 0 );
	}

	// チャンネルマスク
	G->L001e1c = (UWORD)MaskChannel;

	// メモリ解放 (MXDRV_Play copies the data into its own buffers)
	LowFree( mdxbuf );
	LowFree( pdxbuf );

	return (0);

  err_malloc:;
	fprintf( stderr, "メモリ確保に失敗しました。\n" );
	goto err_free;

  err_readmdx:;
	fprintf( stderr, "MDXファイル(%s)の読み込みに失敗しました。\n", mdxfilename.c_str() );
	goto err_free;

  err_brokenmdx:;
	fprintf( stderr, "ファイル(%s)はMDXでないか、壊れています。\n", mdxfilename.c_str() );
	goto err_free;

  err_mdxbuf:;
	fclose( fin );
	fprintf( stderr, "ファイル(%s)を読むMDXバッファが足りません。\n", mdxfilename.c_str() );
	goto err_free;

  err_pdxbuf:;
	fclose( fin );
	fprintf( stderr, "ファイル(%s)を読むPDXバッファが足りません。\n", pdxfilename.c_str() );
	goto err_free;

  err_free:;
	LowFree( mdxbuf );
	LowFree( pdxbuf );
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
   "       [port] Windows/Linux version with X68Sound2025\n"
   " MDXファイルによる演奏をWAVEファイルへ録音します。\n"
   " Usage: mxwav [Option] [MDXファイル]\n"
   " PDX:   PDX検索パスは環境変数\"mxp\"(または\"PDX\")で指定します。\n"
   " Option:"
   "-Ln\tn回ループでフェードアウトします(def=2)。\n"
   "\t-Sn\tサンプリングレートを22050/44100/48000/96000/192000で指定します(def=44100)。\n"
   "\t-Pn\tPDXバッファをKB単位で指定します(def=1024)。\n"
   "\t-Wn\t演奏終了後の待ち時間をms単位で指定します(def=2000)。\n"
   "\t-U\tPCM8を使用しません。\n"
   "\t-A\tAIによるADPCM高音質化を行います(X68SOUND_AI_ENHANCE=1と同じ)。\n"
   "\t-B\tベンチマークモード（WAVEファイルを生成しません）。\n"
   "\t-Ofile\t出力WAVEファイルを指定します(def=MDXファイルの拡張子をWAV化)。\n"
   "\t-Mch\tチャンネルをマスクします(ch=\"12345678\"(FM), \"PQRSTUVW\"(PCM))。\n"
   " X68Sound2025の音質設定(環境変数 X68SOUND_*)が有効です。\n"
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
				if ( (a != 22050) && (a != 44100) && (a != 48000) && (a != 96000) && (a != 192000) ) {
					fprintf( stderr, "-Sは22050, 44100, 48000, 96000, 192000が指定できます。\n" );
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
			  case 'A':
			  case 'a':
				// [port] neural ADPCM enhancement (X68Sound2025, offline only)
#ifdef _WIN32
				_putenv( "X68SOUND_AI_ENHANCE=1" );
#else
				setenv( "X68SOUND_AI_ENHANCE", "1", 1 );
#endif
				break;
			  case 'B':
			  case 'b':
				Bench = TRUE;
				break;
			  case 'O':
			  case 'o':
				OutputWAVFile = &argv[i][2];
				break;
			  case 'M':
			  case 'm':
				{
					char *p = &argv[i][2];
					while (*(p)) {
						int ch;
						int c = toupper((unsigned char)*(p++));
						switch (c) {
						  case '1': case '2': case '3': case '4':
						  case '5': case '6': case '7': case '8':
							ch = c-'1';
							MaskChannel |= (1<<ch);
							break;
						  case 'P': case 'Q': case 'R': case 'S':
						  case 'T': case 'U': case 'V': case 'W':
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
				fprintf( stderr, "オプション(%s)が異常です。\n", argv[i] );
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

#define N_WaveBuf 100
static std::vector<UBYTE> WaveBuf;
static int WaveBufSize;	// samples per 10ms ([port] follows the real output rate)
static volatile int MXDRVStarted;

void onsigint( int )
{
	if ( MXDRVStarted ) {
		MXDRV_Stop();
		MXDRV_End();
	}
	fprintf( stderr, "\nTerminated.\n" );
	exit(EXIT_FAILURE);
}

static void PutLE32( BYTE *w, unsigned long v ) {
	w[0] = (BYTE)(v>>0); w[1] = (BYTE)(v>>8); w[2] = (BYTE)(v>>16); w[3] = (BYTE)(v>>24);
}
static void PutLE16( BYTE *w, unsigned int v ) {
	w[0] = (BYTE)(v>>0); w[1] = (BYTE)(v>>8);
}

int main( int argc, char *argv[] )
{
	FILE *fout = NULL;
	static BYTE wavheader[0x2c];
	unsigned long wavsize;
	unsigned long filesize;
	int postcount = 0;
	unsigned long ct;
	int ti;
	int outrate;

#ifdef _WIN32
	SetConsoleOutputCP( CP_UTF8 );	// [port] messages are UTF-8
#endif

	// CTRL-Cを塞ぐ
	signal( SIGINT, onsigint );

	// 引数解析
	AnalyzeArg( argc, argv );
	if ( MDXFileName == NULL ) {
		PrintHelp();
		exit(EXIT_FAILURE);
	}

	// WAVファイル名作成
	if ( OutputWAVFile.empty() ) {
		std::string base = BaseName( FullPath( MDXFileName ) );
		size_t dot = base.rfind( '.' );
		if ( dot != std::string::npos ) base.resize( dot );
		OutputWAVFile = AddDirMark( CurrentDir() ) + base + ".wav";
	}

	// 出力WAVEファイルを開く
	if ( !Bench ) {
		fout = fopen( OutputWAVFile.c_str(), "wb" );
		if ( fout == NULL ) {
			fprintf( stderr, "出力ファイル(%s)が開けません。\n", OutputWAVFile.c_str() );
			exit(EXIT_FAILURE);
		}
	}

	// 空のwaveヘッダを出力
	if ( fout ) {
		if ( sizeof(wavheader) != fwrite( wavheader, 1, sizeof( wavheader ), fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile.c_str() );
			exit(EXIT_FAILURE);
		}
	}

	// 演奏開始
	X68Sound_InitConfig();	// [port] X68SOUND_* environment (DllMain is not called)
	if ( MXDRV_Start( SampleRate, 0, 500, 5, MDXBuf*1024, PDXBuf*1024, 0 ) ) {
		fprintf( stderr, "MXDRVの初期化に失敗しました。\n" );
		exit(EXIT_FAILURE);
	}
	MXDRVStarted = 1;
	{
		MXCALLBACK_OPMINTFUNC **p;
		p = (MXCALLBACK_OPMINTFUNC **)MXDRV_GetWork( MXDRV_CALLBACK_OPMINT );
		*(p) = &MXCALLBACK_OPMINTfunc;
	}
	FM = (MXWORK_CH *)MXDRV_GetWork( MXDRV_WORK_FM );
	PCM = (MXWORK_CH *)MXDRV_GetWork( MXDRV_WORK_PCM );
	G = (MXWORK_GLOBAL *)MXDRV_GetWork( MXDRV_WORK_GLOBAL );
	OPM = (MXWORK_OPM *)MXDRV_GetWork( MXDRV_WORK_OPM );

	// [port] actual output rate (X68SOUND_OUTPUT_RATE may override -S)
	outrate = X68Sound_GetSamprate();
	if ( outrate <= 0 ) outrate = SampleRate;
	if ( outrate != SampleRate ) {
		fprintf( stderr, "出力サンプリングレート: %dHz (X68SOUND_OUTPUT_RATE)\n", outrate );
	}
	WaveBufSize = outrate/100;	// 10ms分
	WaveBuf.assign( (size_t)WaveBufSize*4*N_WaveBuf, 0 );

	if ( PlayMDX( MDXFileName ) != 0 ) {
		MXDRV_End();
		exit(EXIT_FAILURE);
	}
	fprintf( stderr, "TITLE: %s\n", SjisToUtf8( MDXTitle ).c_str() );

	// 録音
	ct = 0;
	ti = 0;
	wavsize = 0;
	clock_t t0 = clock();
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
				if ( ct*WaveBufSize*4 != fwrite( WaveBuf.data(), 1, ct*WaveBufSize*4, fout ) ) {
					fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile.c_str() );
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
		if ( ct*WaveBufSize*4 != fwrite( WaveBuf.data(), 1, ct*WaveBufSize*4, fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile.c_str() );
			MXDRV_Stop();
			MXDRV_End();
			exit(EXIT_FAILURE);
		}
	}
	{
		double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
		int speed = elapsed > 0 ? (int)((double)ti / elapsed * 100) : 0;
		fprintf( stderr, "%02d:%02d / %ldKB / 生成速度=%d%%              \n", ti/60, ti%60, wavsize/1024, speed );
	}

	// 演奏終了
	MXDRV_Stop();
	MXDRV_End();
	MXDRVStarted = 0;

	// WAVEヘッダ作成
	filesize = wavsize+sizeof(wavheader);
	BYTE *w = &wavheader[0];
	memcpy( w+0x00, "RIFF", 4 );
	PutLE32( w+0x04, filesize-8 );		// ファイルサイズ-8
	memcpy( w+0x08, "WAVE", 4 );
	memcpy( w+0x0c, "fmt ", 4 );
	PutLE32( w+0x10, 0x10 );			// ヘッダサイズ
	PutLE16( w+0x14, 0x01 );			// 0x0001で固定 (PCM)
	PutLE16( w+0x16, 0x02 );			// 出力チャンネル数
	PutLE32( w+0x18, outrate );			// サンプリングレート
	PutLE32( w+0x1c, outrate*4 );		// １秒あたりのバイト数
	PutLE16( w+0x20, 4 );				// １サンプルのバイト数
	PutLE16( w+0x22, 0x10 );			// １データのビット数
	memcpy( w+0x24, "data", 4 );
	PutLE32( w+0x28, wavsize );
	if ( fout ) {
		rewind( fout );
		if ( sizeof(wavheader) != fwrite( wavheader, 1, sizeof( wavheader ), fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile.c_str() );
			exit(EXIT_FAILURE);
		}
	}

	// 出力WAVEファイルを閉じる
	if ( fout ) {
		if ( fclose( fout ) ) {
			fprintf( stderr, "ファイル(%s)出力エラーが発生しました。\n", OutputWAVFile.c_str() );
			exit(EXIT_FAILURE);
		}
	}

	return ( EXIT_SUCCESS );
}

// [EOF]
