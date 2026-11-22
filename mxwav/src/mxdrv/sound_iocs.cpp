#include	<stdio.h>
#include	<stdint.h>

#include	"sound_iocs.h"
#include	"x68sound.h"

// [port] バイトスワップ: 元はMSVCインラインアセンブラ。移植可能な実装に置換。
//        bswapw/bswapl は X68Sound2025 (global.h) 側の実装を使用する。

volatile unsigned char AdpcmStat=0; // $02:adpcmout $12:adpcmaot $22:adpcmlot $32:adpcmcot
volatile unsigned char OpmReg1B=0;  // OPM レジスタ $1B の内容
volatile unsigned char DmaErrCode = 0;

volatile unsigned char *Adpcmcot_adrs;
volatile int	Adpcmcot_len;

// OPMのBUSY待ち
void OpmWait() {
	while (X68Sound_OpmPeek() & 0x80);
}

// IOCS _OPMSET ($68) の処理
// [引数]
//   int addr : OPMレジスタナンバー(0～255)
//   int data : データ(0～255)
void _iocs_opmset(int addr, int data) {
	if (addr == 0x1B) {
		OpmReg1B = (OpmReg1B&0xC0)|(data&0x3F);
		data = OpmReg1B;
	}
	OpmWait();
	X68Sound_OpmReg(addr);
	OpmWait();
	X68Sound_OpmPoke(data);
}

// IOCS _OPMSNS ($69) の処理
// [戻り値]
//   bit 0 : タイマーAオーバーフローのとき1になる
//   bit 1 : タイマーBオーバーフローのとき1になる
//   bit 7 : 0ならばデータ書き込み可能
int _iocs_opmsns() {
	return X68Sound_OpmPeek();
}

void (CALLBACK *OpmIntProc)()=NULL;		// OPMのタイマー割り込み処理アドレス

// IOCS _OPMINTST ($6A) の処理
// [引数]
//   void *addr : 割り込み処理アドレス
//                0のときは割り込み禁止
// [戻り値]
//   割り込みが設定された場合は 0
//   既に割り込みが設定されている場合はその割り込み処理アドレスを返す
int _iocs_opmintst(void *addr) {
	if (addr == 0) {				// 引数が0の時は割り込みを禁止する
		OpmIntProc = NULL;
		X68Sound_OpmInt(OpmIntProc);
		return 0;
	}
	if (OpmIntProc != NULL) {		// 既に設定されている場合は、その処理アドレスを返す
		return (int)(intptr_t)OpmIntProc;	// [port] only used as "already set" flag
	}
	OpmIntProc = (void (CALLBACK *)())addr;
	X68Sound_OpmInt(OpmIntProc);	// OPMの割り込み処理アドレスを設定
	return 0;
}

// DMA転送終了割り込み処理ルーチン
void CALLBACK DmaIntProc() {
	if (AdpcmStat == 0x32 && (X68Sound_DmaPeek(0x00)&0x40)!=0) {	// コンティニューモード時の処理
		X68Sound_DmaPoke(0x00, 0x40);	// BTCビットをクリア
		if (Adpcmcot_len > 0) {
			int dmalen;
			dmalen = Adpcmcot_len;
			if (dmalen > 0xFF00) {	// 1度に転送できるバイト数は0xFF00
				dmalen = 0xFF00;
			}
			X68Sound_DmaPokeL(0x1C, Adpcmcot_adrs);	// BARに次のDMA転送アドレスをセット
			X68Sound_DmaPokeW(0x1A, dmalen);	// BTCに次のDMA転送バイト数をセット
			Adpcmcot_adrs += dmalen;
			Adpcmcot_len -= dmalen;

			X68Sound_DmaPoke(0x07, 0x48);	// コンティニューオペレーション設定
		}
		return;
	}
	if (!(AdpcmStat&0x80)) {
		X68Sound_PpiCtrl(0x01);	// ADPCM右出力OFF
		X68Sound_PpiCtrl(0x03);	// ADPCM左出力OFF
		X68Sound_AdpcmPoke(0x01);	// ADPCM再生動作停止
	}
	AdpcmStat = 0;
	X68Sound_DmaPoke(0x00, 0xFF);	// DMA CSR の全ビットをクリア
}

// DMAエラー割り込み処理ルーチン
void CALLBACK DmaErrIntProc() {
	DmaErrCode = X68Sound_DmaPeek(0x01);	// エラーコードを DmaErrCode に保存

	X68Sound_PpiCtrl(0x01);	// ADPCM右出力OFF
	X68Sound_PpiCtrl(0x03);	// ADPCM左出力OFF
	X68Sound_AdpcmPoke(0x01);	// ADPCM再生動作停止

	AdpcmStat = 0;
	X68Sound_DmaPoke(0x00, 0xFF);	// DMA CSR の全ビットをクリア
}

unsigned char	PANTBL[4] = {3, 1, 2, 0};

// サンプリング周波数とPANを設定してDMA転送を開始するルーチン
// [引数]
//   unsigned short mode : サンプリング周波数*256+PAN
//   unsigned char ccr : DMA CCR に書き込むデータ
void SetAdpcmMode(unsigned short mode, unsigned char ccr) {
	if (mode >= 0x0200) {
		mode -= 0x0200;
		OpmReg1B &= 0x7F;	// ADPCMのクロックは8MHz
	} else {
		OpmReg1B |= 0x80;	// ADPCMのクロックは4MHz
	}
	OpmWait();
	X68Sound_OpmReg(0x1B);
	OpmWait();
	X68Sound_OpmPoke(OpmReg1B);	// ADPCMのクロック設定(8or4MHz)
	unsigned char ppireg;
	ppireg = ((mode>>6)&0x0C) | PANTBL[mode&3];
	ppireg |= (X68Sound_PpiPeek()&0xF0);
	X68Sound_DmaPoke(0x07, ccr);	// DMA転送開始
	X68Sound_PpiPoke(ppireg);	// サンプリングレート＆PANをPPIに設定
}

// _iocs_adpcmoutのメインルーチン
// [引数]
//   unsigned char stat : ADPCMを停止させずに続けてDMA転送を行う場合は$80
//                        DMA転送終了後ADPCMを停止させる場合は$00
//   unsigned short len : DMA転送バイト数
//   unsigned char *adrs : DMA転送アドレス
void AdpcmoutMain(unsigned char stat, unsigned short mode, unsigned short len, unsigned char *adrs) {
	while (AdpcmStat);	// DMA転送終了待ち
	AdpcmStat = stat+2;
	X68Sound_DmaPoke(0x05, 0x32);	// DMA OCR をチェイン動作なしに設定

	X68Sound_DmaPoke(0x00, 0xFF);	// DMA CSR の全ビットをクリア
	X68Sound_DmaPokeL(0x0C, adrs);	// DMA MAR にDMA転送アドレスをセット
	X68Sound_DmaPokeW(0x0A, len);	// DMA MTC にDMA転送バイト数をセット
	SetAdpcmMode(mode, 0x88);	// サンプリング周波数とPANを設定してDMA転送開始

	X68Sound_AdpcmPoke(0x02);	// ADPCM再生開始
}

// IOCS _ADPCMOUT ($60) の処理
// [引数]
//   void *addr : ADPCMデータアドレス
//   int mode : サンプリング周波数(0～4)*256+PAN(0～3)
//   int len : ADPCMデータのバイト数
void _iocs_adpcmout(void *addr, int mode, int len) {
	int dmalen;
	unsigned char *dmaadrs = (unsigned char *)addr;
	while (AdpcmStat);	// DMA転送終了待ち
	while (len > 0x0000FF00) {	// ADPCMデータが0xFF00バイト以上の場合は
		dmalen = 0x0000FF00;	// 0xFF00バイトずつ複数回に分けてDMA転送を行う
		AdpcmoutMain(0x80,mode,dmalen,dmaadrs);
		dmaadrs += dmalen;
		len -= dmalen;
	}
	AdpcmoutMain(0x00,mode,len,dmaadrs);
}

// IOCS _ADPCMAOT ($62) の処理
// [引数]
//   struct _chain *tbl : アレイチェインテーブルのアドレス
//   int mode : サンプリング周波数(0～4)*256+PAN(0～3)
//   int cnt : アレイチェインテーブルのブロック数
void _iocs_adpcmaot(struct _chain *tbl, int mode, int cnt) {
	while (AdpcmStat);	// DMA転送終了待ち

	AdpcmStat = 0x12;
	X68Sound_DmaPoke(0x05, 0x3A);	// DMA OCR をアレイチェイン動作に設定

	X68Sound_DmaPoke(0x00, 0xFF);	// DMA CSR の全ビットをクリア
	X68Sound_DmaPokeL(0x1C, tbl);	// DMA BAR にアレイチェインテーブルアドレスをセット
	X68Sound_DmaPokeW(0x1A, cnt);	// DMA BTC にアレイチェインテーブルの個数をセット
	SetAdpcmMode(mode, 0x88);	// サンプリング周波数とPANを設定してDMA転送開始

	X68Sound_AdpcmPoke(0x02);	// ADPCM再生開始
}

// IOCS _ADPCMAOT ($64) の処理
// [引数]
//   struct _chain2 *tbl : リンクアレイチェインテーブルのアドレス
//   int mode : サンプリング周波数(0～4)*256+PAN(0～3)
void _iocs_adpcmlot(struct _chain2 *tbl, int mode) {
	while (AdpcmStat);	// DMA転送終了待ち

	AdpcmStat = 0x22;
	X68Sound_DmaPoke(0x05, 0x3E);	// DMA OCR をリンクアレイチェイン動作に設定

	X68Sound_DmaPoke(0x00, 0xFF);	// DMA CSR の全ビットをクリア
	X68Sound_DmaPokeL(0x1C, tbl);	// DMA BAR にリンクアレイチェインテーブルアドレスをセット
	SetAdpcmMode(mode, 0x88);	// サンプリング周波数とPANを設定してDMA転送開始

	X68Sound_AdpcmPoke(0x02);	// ADPCM再生開始
}


// コンティニューモードを利用してADPCM出力を行うサンプル
// IOCS _ADPCMOUT と同じ処理を行うが、データバイト数が0xFF00バイト以上でも
// すぐにリターンする。
// [引数]
//   void *addr : ADPCMデータアドレス
//   int mode : サンプリング周波数(0～4)*256+PAN(0～3)
//   int len : ADPCMデータのバイト数
void _iocs_adpcmcot(void *addr, int mode, int len) {
	int dmalen;
	Adpcmcot_adrs = (unsigned char *)addr;
	Adpcmcot_len = len;
	while (AdpcmStat);	// DMA転送終了待ち
	AdpcmStat = 0x32;

	X68Sound_DmaPoke(0x05, 0x32);	// DMA OCR をチェイン動作なしに設定

	dmalen = Adpcmcot_len;
	if (dmalen > 0xFF00) {	// ADPCMデータが0xFF00バイト以上の場合は
		dmalen = 0xFF00;	// 0xFF00バイトずつ複数回に分けてDMA転送を行う
	}

	X68Sound_DmaPoke(0x00, 0xFF);	// DMA CSR の全ビットをクリア
	X68Sound_DmaPokeL(0x0C, Adpcmcot_adrs);	// DMA MAR にDMA転送アドレスをセット
	X68Sound_DmaPokeW(0x0A, dmalen);	// DMA MTC にDMA転送バイト数をセット
	Adpcmcot_adrs += dmalen;
	Adpcmcot_len -= dmalen;
	if (Adpcmcot_len <= 0) {
		SetAdpcmMode(mode, 0x88);	// データバイト数が0xFF00以下の場合は通常転送
	} else {
		dmalen = Adpcmcot_len;
		if (dmalen > 0xFF00) {
			dmalen = 0xFF00;
		}
		X68Sound_DmaPokeL(0x1C, Adpcmcot_adrs);	// BARに次のDMA転送アドレスをセット
		X68Sound_DmaPokeW(0x1A, dmalen);	// BTCに次のDMA転送バイト数をセット
		Adpcmcot_adrs += dmalen;
		Adpcmcot_len -= dmalen;
		SetAdpcmMode(mode, 0xC8);	// DMA CNTビットを1にしてDMA転送開始
	}

	X68Sound_AdpcmPoke(0x02);	// ADPCM再生開始
}

// IOCS _ADPCMSNS ($66) の処理
// [戻り値]
//   0 : 何もしていない
//   $02 : _iocs_adpcmout で出力中
//   $12 : _iocs_adpcmaot で出力中
//   $22 : _iocs_adpcmlot で出力中
//   $32 : _iocs_adpcmcot で出力中
int _iocs_adpcmsns() {
	return (AdpcmStat&0x7F);
}

// IOCS _ADPCMMOD ($67) の処理
// [引数]
//   0 : ADPCM再生 終了
//   1 : ADPCM再生 一時停止
//   2 : ADPCM再生 再開
void _iocs_adpcmmod(int mode) {
	switch (mode) {
	case 0:
		AdpcmStat = 0;
		X68Sound_PpiCtrl(0x01);	// ADPCM右出力OFF
		X68Sound_PpiCtrl(0x03);	// ADPCM左出力OFF
		X68Sound_AdpcmPoke(0x01);	// ADPCM再生動作停止
		X68Sound_DmaPoke(0x07, 0x10);	// DMA SAB=1 (ソフトウェアアボート)
		break;
	case 1:
		X68Sound_DmaPoke(0x07, 0x20);	// DMA HLT=1 (ホルトオペレーション)
		break;
	case 2:
		X68Sound_DmaPoke(0x07, 0x08);	// DMA HLT=0 (ホルトオペレーション解除)
		break;
	}
}


// IOCSコールの初期化
// DMAの割り込みを設定する
void sound_iocs_init() {
	X68Sound_DmaInt(DmaIntProc);
	X68Sound_DmaErrInt(DmaErrIntProc);
}
