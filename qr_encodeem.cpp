#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "qr_encodeem.h"
#include "qr_utils.h"

#define MAX_INPUTDATA  3096 // maximum input data size
#define MAX_QRCODESIZE 4096 // (177*177)/8
/*
// qr_encode_data
// 用  途：データエンコード
// 引  数：誤り訂正レベル、型番(0=自動)、型番自動拡張フラグ、マスキング番号(-1=自動)、エンコードデータ、エンコードデータ長
// 戻り値：エンコード成功時=true、データなし、または容量オーバー時=false
// nLevel    : Level, the ammount of redundancy in the QR code
// nVersion  : Specifies the size of the QR code, 0 means auto select.
// nMaskingNo: Something to do with data masking (see wikipedia)
// lpsSource : Source data
// ncSource  : Source data length, if 0 then assume NULL terminated.
bool qr_encode_data(int nLevel, int nVersion, bool bAutoExtent, int nMaskingNo, const uint8_t * lpsSource, int ncSource) {
	int i, j;

	m_nLevel = nLevel;
	m_nMaskingNo = nMaskingNo;

	// データ長が指定されていない場合は lstrlen によって取得
	int ncLength = ncSource > 0 ? ncSource : strlen((char *) lpsSource);

	if (ncLength == 0) {
		return false; // データなし
  }

  // Version Check
	// バージョン(型番)チェック
	int nEncodeVersion = GetEncodeVersion(nVersion, lpsSource, ncLength);

	if (nEncodeVersion == 0)
		return false; // 容量オーバー

	if (nVersion == 0)
	{
		// 型番自動
		m_nVersion = nEncodeVersion;
	}
	else
	{
		if (nEncodeVersion <= nVersion)
		{
			m_nVersion = nVersion;
		}
		else
		{
			if (bAutoExtent)
				m_nVersion = nEncodeVersion; // バージョン(型番)自動拡張
			else
				return false; // 容量オーバー
		}
	}

  // Terminator Code "0000"
	// ターミネータコード"0000"付加
	int ncDataCodeWord = QR_VersionInfo[m_nVersion].ncDataCodeWord[nLevel];

	int ncTerminater = std::min(4, (ncDataCodeWord * 8) - m_ncDataCodeWordBit);

	if (ncTerminater > 0)
		m_ncDataCodeWordBit = SetBitStream(m_ncDataCodeWordBit, 0, ncTerminater);

	// パディングコード"11101100, 00010001"付加
	uint8_t byPaddingCode = 0xec;

	for (i = (m_ncDataCodeWordBit + 7) / 8; i < ncDataCodeWord; ++i)
	{
		m_byDataCodeWord[i] = byPaddingCode;

		byPaddingCode = (uint8_t)(byPaddingCode == 0xec ? 0x11 : 0xec);
	}

	// 総コードワード算出エリアクリア
	m_ncAllCodeWord = QR_VersionInfo[m_nVersion].ncAllCodeWord;
  memset(m_byAllCodeWord,0,m_ncAllCodeWord);

	int nDataCwIndex = 0; // データコードワード処理位置

	// データブロック分割数
	int ncBlock1 = QR_VersionInfo[m_nVersion].RS_BlockInfo1[nLevel].ncRSBlock;
	int ncBlock2 = QR_VersionInfo[m_nVersion].RS_BlockInfo2[nLevel].ncRSBlock;
	int ncBlockSum = ncBlock1 + ncBlock2;

	int nBlockNo = 0; // 処理中ブロック番号

	// ブロック別データコードワード数
	int ncDataCw1 = QR_VersionInfo[m_nVersion].RS_BlockInfo1[nLevel].ncDataCodeWord;
	int ncDataCw2 = QR_VersionInfo[m_nVersion].RS_BlockInfo2[nLevel].ncDataCodeWord;

	// データコードワードインターリーブ配置
	for (i = 0; i < ncBlock1; ++i)
	{
		for (j = 0; j < ncDataCw1; ++j)
		{
			m_byAllCodeWord[(ncBlockSum * j) + nBlockNo] = m_byDataCodeWord[nDataCwIndex++];
		}

		++nBlockNo;
	}

	for (i = 0; i < ncBlock2; ++i)
	{
		for (j = 0; j < ncDataCw2; ++j)
		{
			if (j < ncDataCw1)
			{
				m_byAllCodeWord[(ncBlockSum * j) + nBlockNo] = m_byDataCodeWord[nDataCwIndex++];
			}
			else
			{
				// ２種目ブロック端数分配置
				m_byAllCodeWord[(ncBlockSum * ncDataCw1) + i]  = m_byDataCodeWord[nDataCwIndex++];
			}	
		}

		++nBlockNo;
	}

	// ブロック別ＲＳコードワード数(※現状では同数)
	int ncRSCw1 = QR_VersionInfo[m_nVersion].RS_BlockInfo1[nLevel].ncAllCodeWord - ncDataCw1;
	int ncRSCw2 = QR_VersionInfo[m_nVersion].RS_BlockInfo2[nLevel].ncAllCodeWord - ncDataCw2;

	/////////////////////////////////////////////////////////////////////////
	// ＲＳコードワード算出

	nDataCwIndex = 0;
	nBlockNo = 0;

	for (i = 0; i < ncBlock1; ++i)
	{
		memset(m_byRSWork,0,sizeof(m_byRSWork));

		memmove(m_byRSWork, m_byDataCodeWord + nDataCwIndex, ncDataCw1);

		GetRSCodeWord(m_byRSWork, ncDataCw1, ncRSCw1);

		// ＲＳコードワード配置
		for (j = 0; j < ncRSCw1; ++j)
		{
			m_byAllCodeWord[ncDataCodeWord + (ncBlockSum * j) + nBlockNo] = m_byRSWork[j];
		}

		nDataCwIndex += ncDataCw1;
		++nBlockNo;
	}

	for (i = 0; i < ncBlock2; ++i)
	{
		memset(m_byRSWork,0,sizeof(m_byRSWork));

		memmove(m_byRSWork, m_byDataCodeWord + nDataCwIndex, ncDataCw2);

		GetRSCodeWord(m_byRSWork, ncDataCw2, ncRSCw2);

		// ＲＳコードワード配置
		for (j = 0; j < ncRSCw2; ++j)
		{
			m_byAllCodeWord[ncDataCodeWord + (ncBlockSum * j) + nBlockNo] = m_byRSWork[j];
		}

		nDataCwIndex += ncDataCw2;
		++nBlockNo;
	}

	m_nSymbolSize = m_nVersion * 4 + 17;

	// モジュール配置
	FormatModule();

	return true;
}


// qr_encode_with_version, this does the basic encoding, with a specified version (if possible)
// 用  途：エンコード時バージョン(型番)取得
// 引  数：調査開始バージョン、エンコードデータ、エンコードデータ長
// 戻り値：バージョン番号（容量オーバー時=0）
int qr_encode_with_version(int nVersion, const uint8_t* lpsSource, int ncLength) {
	int nVerGroup = nVersion >= 27 ? QR_VRESION_L : (nVersion >= 10 ? QR_VRESION_M : QR_VRESION_S);
	int i, j;

  // try different versions in order?
	for (i = nVerGroup; i <= QR_VRESION_L; ++i)
	{
		if (EncodeSourceData(lpsSource, ncLength, i))
		{
			if (i == QR_VRESION_S)
			{
				for (j = 1; j <= 9; ++j)
				{
					if ((m_ncDataCodeWordBit + 7) / 8 <= QR_VersionInfo[j].ncDataCodeWord[m_nLevel])
						return j;
				}
			}
			else if (i == QR_VRESION_M)
			{
				for (j = 10; j <= 26; ++j)
				{
					if ((m_ncDataCodeWordBit + 7) / 8 <= QR_VersionInfo[j].ncDataCodeWord[m_nLevel])
						return j;
				}
			}
			else if (i == QR_VRESION_L)
			{
				for (j = 27; j <= 40; ++j)
				{
					if ((m_ncDataCodeWordBit + 7) / 8 <= QR_VersionInfo[j].ncDataCodeWord[m_nLevel])
						return j;
				}
			}
		}
	}

	return 0;
}
*/

/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetBitStream
// 用  途：ビットセット
// 引  数：挿入位置、ビット配列データ、データビット長(最大16)
// 戻り値：次回挿入位置(バッファオーバー時=-1)
// 備  考：m_byDataCodeWord に結果をセット(要ゼロ初期化)

int SetBitStream(uint8_t *codestream, int nIndex, uint16_t wData, int ncData) {
	int i;

	if (nIndex == -1 || nIndex + ncData > MAX_INPUTDATA * 8)
		return -1;

	for (i = 0; i < ncData; ++i)
	{
		if (wData & (1 << (ncData - i - 1)))
		{
			codestream[(nIndex + i) / 8] |= 1 << (7 - ((nIndex + i) % 8));
		}
	}

	return nIndex + ncData;
}



/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::EncodeSourceData
// 用  途：入力データエンコード
// 引  数：入力データ、入力データ長、バージョン(型番)グループ
// 戻り値：エンコード成功時=true

// This actually does the main data encoding.
bool qr_encode_source_data(const uint8_t* lpsSource,uint8_t *outputdata,int ncLength, int nVerGroup) {

  int32_t m_nBlockLength[MAX_INPUTDATA];
  uint8_t m_byBlockMode[MAX_INPUTDATA];
	int m_ncDataCodeWordBit; // データコードワードビット長
	uint8_t m_byDataCodeWord[MAX_INPUTDATA]; // 入力データエンコードエリア

	int m_ncDataBlock;

	memset(m_nBlockLength,0,sizeof(m_nBlockLength));

	int i, j;

	// どのモードが何文字(バイト)継続しているかを調査
	for (m_ncDataBlock = i = 0; i < ncLength; ++i) {
		uint8_t byMode;

		if (i < ncLength - 1 && IsKanjiData(lpsSource[i], lpsSource[i + 1]))
			byMode = QR_MODE_KANJI;
		else if (IsNumeralData(lpsSource[i]))
			byMode = QR_MODE_NUMERAL;
		else if (IsAlphabetData(lpsSource[i]))
			byMode = QR_MODE_ALPHABET;
		else
			byMode = QR_MODE_8BIT;

		if (i == 0)
			m_byBlockMode[0] = byMode;

		if (m_byBlockMode[m_ncDataBlock] != byMode)
			m_byBlockMode[++m_ncDataBlock] = byMode;

		++m_nBlockLength[m_ncDataBlock];

		if (byMode == QR_MODE_KANJI)
		{
			// 漢字は文字数ではなく	数で記録
			++m_nBlockLength[m_ncDataBlock];
			++i;
		}
	}

	++m_ncDataBlock;

	/////////////////////////////////////////////////////////////////////////
	// 隣接する英数字モードブロックと数字モードブロックの並びをを条件により結合

	int ncSrcBits, ncDstBits; // 元のビット長と単一の英数字モードブロック化した場合のビット長

	int nBlock = 0;

	while (nBlock < m_ncDataBlock - 1)
	{
		int ncJoinFront, ncJoinBehind; // 前後８ビットバイトモードブロックと結合した場合のビット長
		int nJoinPosition = 0; // ８ビットバイトモードブロックとの結合：-1=前と結合、0=結合しない、1=後ろと結合

		// 「数字−英数字」または「英数字−数字」の並び
		if ((m_byBlockMode[nBlock] == QR_MODE_NUMERAL  && m_byBlockMode[nBlock + 1] == QR_MODE_ALPHABET) ||
			(m_byBlockMode[nBlock] == QR_MODE_ALPHABET && m_byBlockMode[nBlock + 1] == QR_MODE_NUMERAL))
		{
			// 元のビット長と単一の英数字モードブロック化した場合のビット長を比較
			ncSrcBits = GetBitLength(m_byBlockMode[nBlock], m_nBlockLength[nBlock], nVerGroup) +
						GetBitLength(m_byBlockMode[nBlock + 1], m_nBlockLength[nBlock + 1], nVerGroup);

			ncDstBits = GetBitLength(QR_MODE_ALPHABET, m_nBlockLength[nBlock] + m_nBlockLength[nBlock + 1], nVerGroup);

			if (ncSrcBits > ncDstBits)
			{
				// 前後に８ビットバイトモードブロックがある場合、それらとの結合が有利かどうかをチェック
				if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_8BIT)
				{
					// 前に８ビットバイトモードブロックあり
					ncJoinFront = GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock - 1] + m_nBlockLength[nBlock], nVerGroup) +
								  GetBitLength(m_byBlockMode[nBlock + 1], m_nBlockLength[nBlock + 1], nVerGroup);

					if (ncJoinFront > ncDstBits + GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock - 1], nVerGroup))
						ncJoinFront = 0; // ８ビットバイトモードブロックとは結合しない
				}
				else
					ncJoinFront = 0;

				if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_8BIT)
				{
					// 後ろに８ビットバイトモードブロックあり
					ncJoinBehind = GetBitLength(m_byBlockMode[nBlock], m_nBlockLength[nBlock], nVerGroup) +
								   GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock + 1] + m_nBlockLength[nBlock + 2], nVerGroup);

					if (ncJoinBehind > ncDstBits + GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock + 2], nVerGroup))
						ncJoinBehind = 0; // ８ビットバイトモードブロックとは結合しない
				}
				else
					ncJoinBehind = 0;

				if (ncJoinFront != 0 && ncJoinBehind != 0)
				{
					// 前後両方に８ビットバイトモードブロックがある場合はデータ長が短くなる方を優先
					nJoinPosition = (ncJoinFront < ncJoinBehind) ? -1 : 1;
				}
				else
				{
					nJoinPosition = (ncJoinFront != 0) ? -1 : ((ncJoinBehind != 0) ? 1 : 0);
				}

				if (nJoinPosition != 0)
				{
					// ８ビットバイトモードブロックとの結合
					if (nJoinPosition == -1)
					{
						m_nBlockLength[nBlock - 1] += m_nBlockLength[nBlock];

						// 後続をシフト
						for (i = nBlock; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}
					}
					else
					{
						m_byBlockMode[nBlock + 1] = QR_MODE_8BIT;
						m_nBlockLength[nBlock + 1] += m_nBlockLength[nBlock + 2];

						// 後続をシフト
						for (i = nBlock + 2; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}
					}

					--m_ncDataBlock;
				}
				else
				{
					// 英数字と数字の並びを単一の英数字モードブロックに統合

					if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_ALPHABET)
					{
						// 結合しようとするブロックの後ろに続く英数字モードブロックを結合
						m_nBlockLength[nBlock + 1] += m_nBlockLength[nBlock + 2];

						// 後続をシフト
						for (i = nBlock + 2; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}

						--m_ncDataBlock;
					}

					m_byBlockMode[nBlock] = QR_MODE_ALPHABET;
					m_nBlockLength[nBlock] += m_nBlockLength[nBlock + 1];

					// 後続をシフト
					for (i = nBlock + 1; i < m_ncDataBlock - 1; ++i)
					{
						m_byBlockMode[i]  = m_byBlockMode[i + 1];
						m_nBlockLength[i] = m_nBlockLength[i + 1];
					}

					--m_ncDataBlock;

					if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_ALPHABET)
					{
						// 結合したブロックの前の英数字モードブロックを結合
						m_nBlockLength[nBlock - 1] += m_nBlockLength[nBlock];

						// 後続をシフト
						for (i = nBlock; i < m_ncDataBlock - 1; ++i)
						{
							m_byBlockMode[i]  = m_byBlockMode[i + 1];
							m_nBlockLength[i] = m_nBlockLength[i + 1];
						}

						--m_ncDataBlock;
					}
				}

				continue; // 現在位置のブロックを再調査
			}
		}

		++nBlock; // 次ブロックを調査
	}

	/////////////////////////////////////////////////////////////////////////
	// 連続する短いモードブロックを８ビットバイトモードブロック化

	nBlock = 0;

	while (nBlock < m_ncDataBlock - 1)
	{
		ncSrcBits = GetBitLength(m_byBlockMode[nBlock], m_nBlockLength[nBlock], nVerGroup)
					+ GetBitLength(m_byBlockMode[nBlock + 1], m_nBlockLength[nBlock + 1], nVerGroup);

		ncDstBits = GetBitLength(QR_MODE_8BIT, m_nBlockLength[nBlock] + m_nBlockLength[nBlock + 1], nVerGroup);

		// 前に８ビットバイトモードブロックがある場合、重複するインジケータ分を減算
		if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_8BIT)
			ncDstBits -= (4 + nIndicatorLen8Bit[nVerGroup]);

		// 後ろに８ビットバイトモードブロックがある場合、重複するインジケータ分を減算
		if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_8BIT)
			ncDstBits -= (4 + nIndicatorLen8Bit[nVerGroup]);

		if (ncSrcBits > ncDstBits)
		{
			if (nBlock >= 1 && m_byBlockMode[nBlock - 1] == QR_MODE_8BIT)
			{
				// 結合するブロックの前にある８ビットバイトモードブロックを結合
				m_nBlockLength[nBlock - 1] += m_nBlockLength[nBlock];

				// 後続をシフト
				for (i = nBlock; i < m_ncDataBlock - 1; ++i)
				{
					m_byBlockMode[i]  = m_byBlockMode[i + 1];
					m_nBlockLength[i] = m_nBlockLength[i + 1];
				}

				--m_ncDataBlock;
				--nBlock;
			}

			if (nBlock < m_ncDataBlock - 2 && m_byBlockMode[nBlock + 2] == QR_MODE_8BIT)
			{
				// 結合するブロックの後ろにある８ビットバイトモードブロックを結合
				m_nBlockLength[nBlock + 1] += m_nBlockLength[nBlock + 2];

				// 後続をシフト
				for (i = nBlock + 2; i < m_ncDataBlock - 1; ++i)
				{
					m_byBlockMode[i]  = m_byBlockMode[i + 1];
					m_nBlockLength[i] = m_nBlockLength[i + 1];
				}

				--m_ncDataBlock;
			}

			m_byBlockMode[nBlock] = QR_MODE_8BIT;
			m_nBlockLength[nBlock] += m_nBlockLength[nBlock + 1];

			// 後続をシフト
			for (i = nBlock + 1; i < m_ncDataBlock - 1; ++i)
			{
				m_byBlockMode[i]  = m_byBlockMode[i + 1];
				m_nBlockLength[i] = m_nBlockLength[i + 1];
			}

			--m_ncDataBlock;

			// 結合したブロックの前から再調査
			if (nBlock >= 1)
				--nBlock;

			continue;
		}

		++nBlock; // 次ブロックを調査
	}

	/////////////////////////////////////////////////////////////////////////
	// ビット配列化
	int ncComplete = 0; // 処理済データカウンタ
	uint16_t wBinCode;

	m_ncDataCodeWordBit = 0; // ビット単位処理カウンタ

  for(int n=0;n<MAX_INPUTDATA;n++) m_nBlockLength[n]=0;

	for (i = 0; i < m_ncDataBlock && m_ncDataCodeWordBit != -1; ++i)
	{
		if (m_byBlockMode[i] == QR_MODE_NUMERAL)
		{
			/////////////////////////////////////////////////////////////////
			// 数字モード

			// インジケータ(0001b)
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, 1, 4); 

			// 文字数セット
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit,
                                         (uint16_t)m_nBlockLength[i],
                                         nIndicatorLenNumeral[nVerGroup]);

			// ビット列保存
			for (j = 0; j < m_nBlockLength[i]; j += 3)
			{
				if (j < m_nBlockLength[i] - 2)
				{
					wBinCode = (uint16_t)(((lpsSource[ncComplete + j]	  - '0') * 100) +
									  ((lpsSource[ncComplete + j + 1] - '0') * 10) +
									   (lpsSource[ncComplete + j + 2] - '0'));

					m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, wBinCode, 10);
				}
				else if (j == m_nBlockLength[i] - 2)
				{
					// 端数２バイト
					wBinCode = (uint16_t)(((lpsSource[ncComplete + j] - '0') * 10) +
									   (lpsSource[ncComplete + j + 1] - '0'));

					m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, wBinCode, 7);
				}
				else if (j == m_nBlockLength[i] - 1)
				{
					// 端数１バイト
					wBinCode = (uint16_t)(lpsSource[ncComplete + j] - '0');

					m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, wBinCode, 4);
				}
			}

			ncComplete += m_nBlockLength[i];
		}

		else if (m_byBlockMode[i] == QR_MODE_ALPHABET)
		{
			/////////////////////////////////////////////////////////////////
			// 英数字モード

			// モードインジケータ(0010b)
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, 2, 4);

			// 文字数セット
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, (uint16_t)m_nBlockLength[i], nIndicatorLenAlphabet[nVerGroup]);

			// ビット列保存
			for (j = 0; j < m_nBlockLength[i]; j += 2)
			{
				if (j < m_nBlockLength[i] - 1)
				{
					wBinCode = (uint16_t)((AlphabetToBinary(lpsSource[ncComplete + j]) * 45) +
									   AlphabetToBinary(lpsSource[ncComplete + j + 1]));

					m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, wBinCode, 11);
				}
				else
				{
					// 端数１バイト
					wBinCode = (uint16_t)AlphabetToBinary(lpsSource[ncComplete + j]);

					m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, wBinCode, 6);
				}
			}

			ncComplete += m_nBlockLength[i];
		}

		else if (m_byBlockMode[i] == QR_MODE_8BIT)
		{
			/////////////////////////////////////////////////////////////////
			// ８ビットバイトモード

			// モードインジケータ(0100b)
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, 4, 4);

			// 文字数セット
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, (uint16_t)m_nBlockLength[i], nIndicatorLen8Bit[nVerGroup]);

			// ビット列保存
			for (j = 0; j < m_nBlockLength[i]; ++j)
			{
				m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, (uint16_t)lpsSource[ncComplete + j], 8);
			}

			ncComplete += m_nBlockLength[i];
		}
		else // m_byBlockMode[i] == QR_MODE_KANJI
		{
			/////////////////////////////////////////////////////////////////
			// 漢字モード

			// モードインジケータ(1000b)
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, 8, 4);

			// 文字数セット
			m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, (uint16_t)(m_nBlockLength[i] / 2), nIndicatorLenKanji[nVerGroup]);

			// 漢字モードでビット列保存
			for (j = 0; j < m_nBlockLength[i] / 2; ++j)
			{
				uint16_t wBinCode = KanjiToBinary((uint16_t)(((uint8_t)lpsSource[ncComplete + (j * 2)] << 8) + (uint8_t)lpsSource[ncComplete + (j * 2) + 1]));

				m_ncDataCodeWordBit = SetBitStream(m_byDataCodeWord,m_ncDataCodeWordBit, wBinCode, 13);
			}

			ncComplete += m_nBlockLength[i];
		}
	}

	return (m_ncDataCodeWordBit != -1);
}






/*
/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::GetRSCodeWord
// 用  途：ＲＳ誤り訂正コードワード取得
// 引  数：データコードワードアドレス、データコードワード長、ＲＳコードワード長
// 備  考：総コードワード分のエリアを確保してから呼び出し

void CQR_Encode::GetRSCodeWord(uint8_t *lpbyRSWork, int ncDataCodeWord, int ncRSCodeWord)
{
	int i, j;

	for (i = 0; i < ncDataCodeWord ; ++i)
	{
		if (lpbyRSWork[0] != 0)
		{
			uint8_t nExpFirst = byIntToExp[lpbyRSWork[0]]; // 初項係数より乗数算出

			for (j = 0; j < ncRSCodeWord; ++j)
			{
				// 各項乗数に初項乗数を加算（% 255 → α^255 = 1）
				uint8_t nExpElement = (uint8_t)(((int)(byRSExp[ncRSCodeWord][j] + nExpFirst)) % 255);

				// 排他論理和による剰余算出
				lpbyRSWork[j] = (uint8_t)(lpbyRSWork[j + 1] ^ byExpToInt[nExpElement]);
			}

			// 残り桁をシフト
			for (j = ncRSCodeWord; j < ncDataCodeWord + ncRSCodeWord - 1; ++j)
				lpbyRSWork[j] = lpbyRSWork[j + 1];
		}
		else
		{
			// 残り桁をシフト
			for (j = 0; j < ncDataCodeWord + ncRSCodeWord - 1; ++j)
				lpbyRSWork[j] = lpbyRSWork[j + 1];
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::FormatModule
// 用  途：モジュールへのデータ配置
// 戻り値：一辺のモジュール数

void CQR_Encode::FormatModule()
{
	int i, j;

  clear_qrimage();
	//memset(m_byModuleData,0,sizeof(m_byModuleData));

	// 機能モジュール配置
	SetFunctionModule();

	// データパターン配置
	SetCodeWordPattern();

	if (m_nMaskingNo == -1)
	{
		// 最適マスキングパターン選択
		m_nMaskingNo = 0;

		SetMaskingPattern(m_nMaskingNo); // マスキング
		SetFormatInfoPattern(m_nMaskingNo); // フォーマット情報パターン配置

		int nMinPenalty = CountPenalty();

    // try different masking patterns, the one with the lowest penalty wins!
		for (i = 1; i <= 7; ++i)
		{
			SetMaskingPattern(i); // マスキング
			SetFormatInfoPattern(i); // フォーマット情報パターン配置

			int nPenalty = CountPenalty();

			if (nPenalty < nMinPenalty)
			{
				nMinPenalty = nPenalty;
				m_nMaskingNo = i;
			}
		}
	}

	SetMaskingPattern(m_nMaskingNo); // マスキング
	SetFormatInfoPattern(m_nMaskingNo); // フォーマット情報パターン配置

	// モジュールパターンをブール値に変換
	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize; ++j)
		{
      set_qrimage_data(i,j,1);
//			m_byModuleData[i][j] = (uint8_t)((m_byModuleData[i][j] & 0x11) != 0);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetFunctionModule
// 用  途：機能モジュール配置
// 備  考：フォーマット情報は機能モジュール登録のみ(実データは空白)

void CQR_Encode::SetFunctionModule()
{
	int i, j;

	// 位置検出パターン
	SetFinderPattern(0, 0);
	SetFinderPattern(m_nSymbolSize - 7, 0);
	SetFinderPattern(0, m_nSymbolSize - 7);

	// 位置検出パターンセパレータ
	for (i = 0; i < 8; ++i) {
    set_qrimage(i,7,1);
    set_qrimage(7,i,1);
//	 	m_byModuleData[i][7] = m_byModuleData[7][i] = '\x20';
    set_qrimage(m_nSymbolSize-8,i) = 1;
    set_qrimage(m_nSymbolSize-8+i,7) = 1;
//		m_byModuleData[m_nSymbolSize - 8][i] = m_byModuleData[m_nSymbolSize - 8 + i][7] = '\x20';


    set_qrimage(i,m_nSymbolSize-8  ) = 1;
    set_qrimage(7,m_nSymbolSize-8+i) = 1;
//		m_byModuleData[i][m_nSymbolSize - 8] = m_byModuleData[7][m_nSymbolSize - 8 + i] = '\x20';
	}

	// フォーマット情報記述位置を機能モジュール部として登録
	for (i = 0; i < 9; ++i)
	{
    set_qrimage(i,8,1);
    set_qrimage(8,i,1);
//		m_byModuleData[i][8] = m_byModuleData[8][i] = '\x20';
	}

	for (i = 0; i < 8; ++i)
	{
    set_qrimage(m_nSymbolSize-8+i,8,1);
    set_qrimage(8,m_nSymbolSize-8+i,1);
//		m_byModuleData[m_nSymbolSize - 8 + i][8] = m_byModuleData[8][m_nSymbolSize - 8 + i] = '\x20';
	}

	// バージョン情報パターン
	SetVersionPattern();

	// 位置合わせパターン
	for (i = 0; i < QR_VersionInfo[m_nVersion].ncAlignPoint; ++i)
	{
		SetAlignmentPattern(QR_VersionInfo[m_nVersion].nAlignPoint[i], 6);
		SetAlignmentPattern(6, QR_VersionInfo[m_nVersion].nAlignPoint[i]);

		for (j = 0; j < QR_VersionInfo[m_nVersion].ncAlignPoint; ++j)
		{
			SetAlignmentPattern(QR_VersionInfo[m_nVersion].nAlignPoint[i], QR_VersionInfo[m_nVersion].nAlignPoint[j]);
		}
	}

  // Timing Pattern
	// タイミングパターン
	for (i = 8; i <= m_nSymbolSize - 9; ++i)
	{
    set_qrimage(i,6,1);
    set_qrimage(6,i,1);
//		m_byModuleData[i][6] = (i % 2) == 0 ? '\x30' : '\x20';
//		m_byModuleData[6][i] = (i % 2) == 0 ? '\x30' : '\x20';
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetFinderPattern
// 用  途：位置検出パターン配置
// 引  数：配置左上座標

void CQR_Encode::SetFinderPattern(int x, int y)
{
	static uint8_t byPattern[] = {0x7f,  // 1111111b
							   0x41,  // 1000001b
							   0x5d,  // 1011101b
							   0x5d,  // 1011101b
							   0x5d,  // 1011101b
							   0x41,  // 1000001b
							   0x7f}; // 1111111b
	int i, j;

	for (i = 0; i < 7; ++i)
	{
		for (j = 0; j < 7; ++j)
		{
      set_qrimage(x+j,y+i,1);
//			m_byModuleData[x + j][y + i] = (byPattern[i] & (1 << (6 - j))) ? '\x30' : '\x20'; 
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetAlignmentPattern
// 用  途：位置合わせパターン配置
// 引  数：配置中央座標

void CQR_Encode::SetAlignmentPattern(int x, int y)
{
	static uint8_t byPattern[] = {0x1f,  // 11111b
							   0x11,  // 10001b
							   0x15,  // 10101b
							   0x11,  // 10001b
							   0x1f}; // 11111b
	int i, j;

//	if (m_byModuleData[x][y] & 0x20)
//		return; // 機能モジュールと重複するため除外

	x -= 2; y -= 2; // 左上隅座標に変換

	for (i = 0; i < 5; ++i)
	{
		for (j = 0; j < 5; ++j)
		{
      set_qrimage(x+j,y+i,1);
			//m_byModuleData[x + j][y + i] = (byPattern[i] & (1 << (4 - j))) ? '\x30' : '\x20'; 
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetVersionPattern
// 用  途：バージョン(型番)情報パターン配置
// 備  考：拡張ＢＣＨ(18,6)符号を誤り訂正として使用

void CQR_Encode::SetVersionPattern()
{
	int i, j;

	if (m_nVersion <= 6)
		return;

	int nVerData = m_nVersion << 12;

	// 剰余ビット算出
	for (i = 0; i < 6; ++i)
	{
		if (nVerData & (1 << (17 - i)))
		{
			nVerData ^= (0x1f25 << (5 - i));
		}
	}

	nVerData += m_nVersion << 12;

	for (i = 0; i < 6; ++i)
	{
		for (j = 0; j < 3; ++j)
		{
      set_qrimage(m_nSymbolSize-11+j,i,1);
      set_qrimage(i,m_nSymbolSize-11+j,1);
			//m_byModuleData[m_nSymbolSize - 11 + j][i] = m_byModuleData[i][m_nSymbolSize - 11 + j] =
			//(nVerData & (1 << (i * 3 + j))) ? '\x30' : '\x20';
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetCodeWordPattern
// 用  途：データパターン配置

void CQR_Encode::SetCodeWordPattern()
{
	int x = m_nSymbolSize;
	int y = m_nSymbolSize - 1;

	int nCoef_x = 1; // ｘ軸配置向き
	int nCoef_y = 1; // ｙ軸配置向き

	int i, j;

	for (i = 0; i < m_ncAllCodeWord; ++i)
	{
		for (j = 0; j < 8; ++j)
		{
			do
			{
				x += nCoef_x;
				nCoef_x *= -1;

				if (nCoef_x < 0)
				{
					y += nCoef_y;

					if (y < 0 || y == m_nSymbolSize)
					{
						y = (y < 0) ? 0 : m_nSymbolSize - 1;
						nCoef_y *= -1;

						x -= 2;

						if (x == 6) // タイミングパターン
							--x;
					}
				}
			}
			//while (m_byModuleData[x][y] & 0x20); // 機能モジュールを除外
  
      if(m_byAllCodeWord[i] & (1 << (7-j))) set_qrimage(x,y,1); else set_qrimage(x,y,0);
			//m_byModuleData[x][y] = (m_byAllCodeWord[i] & (1 << (7 - j))) ? '\x02' : '\x00';
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetMaskingPattern
// 用  途：マスキングパターン配置
// 引  数：マスキングパターン番号

void CQR_Encode::SetMaskingPattern(int nPatternNo)
{
	int i, j;

	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize; ++j)
		{
			if (! (m_byModuleData[j][i] & 0x20)) // 機能モジュールを除外
			{
				bool bMask;

				switch (nPatternNo)
				{
				case 0:
					bMask = ((i + j) % 2 == 0);
					break;

				case 1:
					bMask = (i % 2 == 0);
					break;

				case 2:
					bMask = (j % 3 == 0);
					break;

				case 3:
					bMask = ((i + j) % 3 == 0);
					break;

				case 4:
					bMask = (((i / 2) + (j / 3)) % 2 == 0);
					break;

				case 5:
					bMask = (((i * j) % 2) + ((i * j) % 3) == 0);
					break;

				case 6:
					bMask = ((((i * j) % 2) + ((i * j) % 3)) % 2 == 0);
					break;

				default: // case 7:
					bMask = ((((i * j) % 3) + ((i + j) % 2)) % 2 == 0);
					break;
				}

				m_byModuleData[j][i] = (uint8_t)((m_byModuleData[j][i] & 0xfe) | (((m_byModuleData[j][i] & 0x02) > 1) ^ bMask));
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::SetFormatInfoPattern
// 用  途：フォーマット情報配置
// 引  数：マスキングパターン番号

void CQR_Encode::SetFormatInfoPattern(int nPatternNo)
{
	int nFormatInfo;
	int i;

	switch (m_nLevel)
	{
	case QR_LEVEL_M:
		nFormatInfo = 0x00; // 00nnnb
		break;

	case QR_LEVEL_L:
		nFormatInfo = 0x08; // 01nnnb
		break;

	case QR_LEVEL_Q:
		nFormatInfo = 0x18; // 11nnnb
		break;

	default: // case QR_LEVEL_H:
		nFormatInfo = 0x10; // 10nnnb
		break;
	}

	nFormatInfo += nPatternNo;

	int nFormatData = nFormatInfo << 10;

	// 剰余ビット算出
	for (i = 0; i < 5; ++i)
	{
		if (nFormatData & (1 << (14 - i)))
		{
			nFormatData ^= (0x0537 << (4 - i)); // 10100110111b
		}
	}

	nFormatData += nFormatInfo << 10;

	// マスキング
	nFormatData ^= 0x5412; // 101010000010010b

	// 左上位置検出パターン周り配置
	for (i = 0; i <= 5; ++i)
		m_byModuleData[8][i] = (nFormatData & (1 << i)) ? '\x30' : '\x20';

	m_byModuleData[8][7] = (nFormatData & (1 << 6)) ? '\x30' : '\x20';
	m_byModuleData[8][8] = (nFormatData & (1 << 7)) ? '\x30' : '\x20';
	m_byModuleData[7][8] = (nFormatData & (1 << 8)) ? '\x30' : '\x20';

	for (i = 9; i <= 14; ++i)
		m_byModuleData[14 - i][8] = (nFormatData & (1 << i)) ? '\x30' : '\x20';

	// 右上位置検出パターン下配置
	for (i = 0; i <= 7; ++i)
		m_byModuleData[m_nSymbolSize - 1 - i][8] = (nFormatData & (1 << i)) ? '\x30' : '\x20';

	// 左下位置検出パターン右配置
	m_byModuleData[8][m_nSymbolSize - 8] = '\x30'; // 固定暗モジュール

	for (i = 8; i <= 14; ++i)
		m_byModuleData[8][m_nSymbolSize - 15 + i] = (nFormatData & (1 << i)) ? '\x30' : '\x20';
}


/////////////////////////////////////////////////////////////////////////////
// CQR_Encode::CountPenalty
// 用  途：マスク後ペナルティスコア算出

int CQR_Encode::CountPenalty()
{
	int nPenalty = 0;
	int i, j, k;

	// 同色の列の隣接モジュール
	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize - 4; ++j)
		{
			int nCount = 1;

			for (k = j + 1; k < m_nSymbolSize; k++)
			{
				if (((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i][k] & 0x11) == 0))
					++nCount;
				else
					break;
			}

			if (nCount >= 5)
			{
				nPenalty += 3 + (nCount - 5);
			}

			j = k - 1;
		}
	}

	// 同色の行の隣接モジュール
	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize - 4; ++j)
		{
			int nCount = 1;

			for (k = j + 1; k < m_nSymbolSize; k++)
			{
				if (((m_byModuleData[j][i] & 0x11) == 0) == ((m_byModuleData[k][i] & 0x11) == 0))
					++nCount;
				else
					break;
			}

			if (nCount >= 5)
			{
				nPenalty += 3 + (nCount - 5);
			}

			j = k - 1;
		}
	}

	// 同色のモジュールブロック（２×２）
	for (i = 0; i < m_nSymbolSize - 1; ++i)
	{
		for (j = 0; j < m_nSymbolSize - 1; ++j)
		{
			if ((((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i + 1][j]		& 0x11) == 0)) &&
				(((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i]	[j + 1] & 0x11) == 0)) &&
				(((m_byModuleData[i][j] & 0x11) == 0) == ((m_byModuleData[i + 1][j + 1] & 0x11) == 0)))
			{
				nPenalty += 3;
			}
		}
	}

	// 同一列における 1:1:3:1:1 比率（暗:明:暗:明:暗）のパターン
	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize - 6; ++j)
		{
			if (((j == 0) ||				 (! (m_byModuleData[i][j - 1] & 0x11))) && // 明 または シンボル外
											 (   m_byModuleData[i][j]     & 0x11)   && // 暗 - 1
											 (! (m_byModuleData[i][j + 1] & 0x11))  && // 明 - 1
											 (   m_byModuleData[i][j + 2] & 0x11)   && // 暗 ┐
											 (   m_byModuleData[i][j + 3] & 0x11)   && // 暗 │3
											 (   m_byModuleData[i][j + 4] & 0x11)   && // 暗 ┘
											 (! (m_byModuleData[i][j + 5] & 0x11))  && // 明 - 1
											 (   m_byModuleData[i][j + 6] & 0x11)   && // 暗 - 1
				((j == m_nSymbolSize - 7) || (! (m_byModuleData[i][j + 7] & 0x11))))   // 明 または シンボル外
			{
				// 前または後に4以上の明パターン
				if (((j < 2 || ! (m_byModuleData[i][j - 2] & 0x11)) && 
					 (j < 3 || ! (m_byModuleData[i][j - 3] & 0x11)) &&
					 (j < 4 || ! (m_byModuleData[i][j - 4] & 0x11))) ||
					((j >= m_nSymbolSize - 8  || ! (m_byModuleData[i][j + 8]  & 0x11)) &&
					 (j >= m_nSymbolSize - 9  || ! (m_byModuleData[i][j + 9]  & 0x11)) &&
					 (j >= m_nSymbolSize - 10 || ! (m_byModuleData[i][j + 10] & 0x11))))
				{
					nPenalty += 40;
				}
			}
		}
	}

	// 同一行における 1:1:3:1:1 比率（暗:明:暗:明:暗）のパターン
	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize - 6; ++j)
		{
			if (((j == 0) ||				 (! (m_byModuleData[j - 1][i] & 0x11))) && // 明 または シンボル外
											 (   m_byModuleData[j]    [i] & 0x11)   && // 暗 - 1
											 (! (m_byModuleData[j + 1][i] & 0x11))  && // 明 - 1
											 (   m_byModuleData[j + 2][i] & 0x11)   && // 暗 ┐
											 (   m_byModuleData[j + 3][i] & 0x11)   && // 暗 │3
											 (   m_byModuleData[j + 4][i] & 0x11)   && // 暗 ┘
											 (! (m_byModuleData[j + 5][i] & 0x11))  && // 明 - 1
											 (   m_byModuleData[j + 6][i] & 0x11)   && // 暗 - 1
				((j == m_nSymbolSize - 7) || (! (m_byModuleData[j + 7][i] & 0x11))))   // 明 または シンボル外
			{
				// 前または後に4以上の明パターン
				if (((j < 2 || ! (m_byModuleData[j - 2][i] & 0x11)) && 
					 (j < 3 || ! (m_byModuleData[j - 3][i] & 0x11)) &&
					 (j < 4 || ! (m_byModuleData[j - 4][i] & 0x11))) ||
					((j >= m_nSymbolSize - 8  || ! (m_byModuleData[j + 8][i]  & 0x11)) &&
					 (j >= m_nSymbolSize - 9  || ! (m_byModuleData[j + 9][i]  & 0x11)) &&
					 (j >= m_nSymbolSize - 10 || ! (m_byModuleData[j + 10][i] & 0x11))))
				{
					nPenalty += 40;
				}
			}
		}
	}

	// 全体に対する暗モジュールの占める割合
	int nCount = 0;

	for (i = 0; i < m_nSymbolSize; ++i)
	{
		for (j = 0; j < m_nSymbolSize; ++j)
		{
			if (! (m_byModuleData[i][j] & 0x11))
			{
				++nCount;
			}
		}
	}

	nPenalty += (abs(50 - ((nCount * 100) / (m_nSymbolSize * m_nSymbolSize))) / 5) * 10;

	return nPenalty;
}
*/
      

int qr_getmodule(uint8_t *outputdata,int x,int y) {

  int bitpos = x*y;

  int byte = (x*y)%8;
  int bit  = bitpos-byte;

  if(outputdata[byte] & (1<bit)) {return 1;}
                            else {return 0;}
}

int main() {
  char   *inputdata = "TESTTESTTESTTEST";

  uint8_t outputdata[4096];

  bool ok = qr_encode_source_data((uint8_t *) inputdata,outputdata,16,4);

  if(ok == false) printf("Encoding error\n");

  int width=29;
  for(int y=0;y<width;y++) {
    for(int x=0;x<width;x++) {
      int i = qr_getmodule(outputdata,x,y);
      if(i != 0) {printf("1");}
            else {printf("0");}
    }
    printf("\n");
  }
}
