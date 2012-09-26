// QR_Encode.h : CQR_Encode クラス宣言およびインターフェイス定義
// Date 2006/05/17	Ver. 1.22	Psytec Inc.

#if !defined(AFX_QR_ENCODE_H__AC886DF7_C0AE_4C9F_AC7A_FCDA8CB1DD37__INCLUDED_)
#define AFX_QR_ENCODE_H__AC886DF7_C0AE_4C9F_AC7A_FCDA8CB1DD37__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////
// 定数

// 誤り訂正レベル
#define QR_LEVEL_L	0
#define QR_LEVEL_M	1
#define QR_LEVEL_Q	2
#define QR_LEVEL_H	3

// データモード
#define QR_MODE_NUMERAL		0
#define QR_MODE_ALPHABET	1
#define QR_MODE_8BIT		2
#define QR_MODE_KANJI		3

// バージョン(型番)グループ
#define QR_VRESION_S	0 // 1 〜 9
#define QR_VRESION_M	1 // 10 〜 26
#define QR_VRESION_L	2 // 27 〜 40

#define MAX_ALLCODEuint16_t	 3706 // 総コードワード数最大値
#define MAX_DATACODEuint16_t 2956 // データコードワード最大値(バージョン40-L)
#define MAX_CODEBLOCK	  153 // ブロックデータコードワード数最大値(ＲＳコードワードを含む)
#define MAX_MODULESIZE	  177 // 一辺モジュール数最大値

// ビットマップ描画時マージン
#define QR_MARGIN	4


/////////////////////////////////////////////////////////////////////////////
typedef struct tagRS_BLOCKINFO
{
	int ncRSBlock;		// ＲＳブロック数
	int ncAllCodeWord;	// ブロック内コードワード数
	int ncDataCodeWord;	// データコードワード数(コードワード数 - ＲＳコードワード数)

} RS_BLOCKINFO, *LPRS_BLOCKINFO;


/////////////////////////////////////////////////////////////////////////////
// QRコードバージョン(型番)関連情報

typedef struct tagQR_VERSIONINFO
{
	int nVersionNo;	   // バージョン(型番)番号(1〜40)
	int ncAllCodeWord; // 総コードワード数

	// 以下配列添字は誤り訂正率(0 = L, 1 = M, 2 = Q, 3 = H) 
	int ncDataCodeWord[4];	// データコードワード数(総コードワード数 - ＲＳコードワード数)

	int ncAlignPoint;	// アライメントパターン座標数
	int nAlignPoint[6];	// アライメントパターン中心座標

	RS_BLOCKINFO RS_BlockInfo1[4]; // ＲＳブロック情報(1)
	RS_BLOCKINFO RS_BlockInfo2[4]; // ＲＳブロック情報(2)

} QR_VERSIONINFO, *LPQR_VERSIONINFO;


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode クラス

class CQR_Encode
{
// 構築/消滅
public:
	CQR_Encode();
	~CQR_Encode();

public:
	int m_nLevel;		// 誤り訂正レベル
	int m_nVersion;		// バージョン(型番)
	bool m_bAutoExtent;	// バージョン(型番)自動拡張指定フラグ
	int m_nMaskingNo;	// マスキングパターン番号

public:
	int m_nSymbleSize;
	uint8_t m_byModuleData[MAX_MODULESIZE][MAX_MODULESIZE]; // [x][y]
	// bit5:機能モジュール（マスキング対象外）フラグ: Functional module, not subject to masking
	// bit4:機能モジュール描画データ: Drawing data function module
	// bit1:エンコードデータ: Encoded data
	// bit0:マスク後エンコード描画データ: Data after masking
	// 20hとの論理和により機能モジュール判定、11hとの論理和により描画（最終的にはbool値化）
  // Awful google translate of above line:
  // Determined by the logical sum of the function module 20h, drawing (and eventually binarization bool).
  // Drawing (and eventually binarization bool) The logical sum of the 11h

private:
	int m_ncDataCodeWordBit; // データコードワードビット長
	uint8_t m_byDataCodeWord[MAX_DATACODEuint16_t]; // 入力データエンコードエリア

	int m_ncDataBlock;
	uint8_t m_byBlockMode[MAX_DATACODEuint16_t];
	int m_nBlockLength[MAX_DATACODEuint16_t];

	int m_ncAllCodeWord; // 総コードワード数(ＲＳ誤り訂正データを含む)
	uint8_t m_byAllCodeWord[MAX_ALLCODEuint16_t]; // 総コードワード算出エリア
	uint8_t m_byRSWork[MAX_CODEBLOCK]; // ＲＳコードワード算出ワーク

// データエンコード関連ファンクション
public:
	bool EncodeData(int nLevel, int nVersion, bool bAutoExtent, int nMaskingNo, const uint8_t* lpsSource, int ncSource = 0);

private:
	int GetEncodeVersion(int nVersion, const uint8_t* lpsSource, int ncLength);
	bool EncodeSourceData(const uint8_t* lpsSource, int ncLength, int nVerGroup);

	int GetBitLength(uint8_t nMode, int ncData, int nVerGroup);

	int SetBitStream(int nIndex, uint16_t wData, int ncData);

	bool IsNumeralData(unsigned char c);
	bool IsAlphabetData(unsigned char c);
	bool IsKanjiData(unsigned char c1, unsigned char c2);

	uint8_t AlphabetToBinary(unsigned char c);
	uint16_t KanjiToBinary(uint16_t wc);

	void GetRSCodeWord(uint8_t *lpbyRSWork, int ncDataCodeWord, int ncRSCodeWord);

// モジュール配置関連ファンクション
private:
	void FormatModule();
	void SetFunctionModule();
	void SetFinderPattern(int x, int y);
	void SetAlignmentPattern(int x, int y);
	void SetVersionPattern();
	void SetCodeWordPattern();
	void SetMaskingPattern(int nPatternNo);
	void SetFormatInfoPattern(int nPatternNo);
	int CountPenalty();
};

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_QR_ENCODE_H__AC886DF7_C0AE_4C9F_AC7A_FCDA8CB1DD37__INCLUDED_)
