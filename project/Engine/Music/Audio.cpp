#include "Audio.h"
#include "iostream"

#pragma comment(lib,"xaudio2.lib")

#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// 指揮者
Microsoft::WRL::ComPtr<IXAudio2> Audio::xAudio2_;
// スピーカー
IXAudio2MasteringVoice* Audio::masterVoice_;// これはComptr無理

void Audio::Initialize() {
	HRESULT hr;
	// XAudio2を作成
	hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	// 
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
}

void Audio::Finalize() {
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}
	xAudio2_.Reset();
}

SoundData Audio::LoadWave(const std::string& filename) {
	// 1, ファイルを開く
	// [ ファイル入力ストリームのインスタンス ]
	std::ifstream file;
	// [ .wavファイルをバイナリモードで開く ]
	file.open(filename, std::ios_base::binary);
	// [ ファイルが開けなかったらエラー ]
	assert(file.is_open());
	// .wavを読み込む
	// [ ヘッダ情報を読み込む ]
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	// [ ファイルがRIFFか確認 ]
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	// [ ファイルがWAVEか確認 ]
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}
	// Formatチャンクの読み込み
	FormatChunk format = {};
	// チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}
	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);
	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	// JUNKKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}
	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}
	// Dataチャンクのデータ部(波形データ)の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);
	
	// 3, 読み込み終了
	file.close();
	
	// 4, 音データを作成
	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

//WAVEFORMATEX Audio::MakeWaveFmt() {
//	WAVEFORMATEX wfex{};
//	wfex.wFormatTag = WAVE_FORMAT_PCM;
//	wfex.nChannels = 2; // ステレオ
//	wfex.nSamplesPerSec = 44100; // 44.1kHz
//	wfex.nAvgBytesPerSec = 44100 * 2 * sizeof(short);
//	wfex.nBlockAlign = 2 * sizeof(short);
//	wfex.wBitsPerSample = 16; // 16bit
//	wfex.cbSize = 0;
//
//	return wfex;
//}



const SoundData MediaAudioDecoder::DecodeAudioFile(const std::wstring& filePath)
{
	Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader;
	Microsoft::WRL::ComPtr<IMFMediaType> audioTypeOut;

	// Media Foundation 初期化
	MFStartup(MF_VERSION);

	// ソースリーダー作成
	HRESULT hr = MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &sourceReader);
	if (FAILED(hr)) throw std::runtime_error("Failed to create source reader");

	// 出力形式をPCMに設定
	MFCreateMediaType(&audioTypeOut);
	audioTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	audioTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM); // LPCM
	sourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, audioTypeOut.Get());

	// メディアタイプ取得
	Microsoft::WRL::ComPtr<IMFMediaType> nativeType;
	sourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &nativeType);

	// 波形フォーマット取得
	WAVEFORMATEX* pWaveFormat = nullptr;
	UINT32 size = 0;
	MFCreateWaveFormatExFromMFMediaType(nativeType.Get(), &pWaveFormat, &size);

	std::vector<BYTE> pcmBuffer;
	DWORD streamIndex, flags;
	LONGLONG timestamp;
	Microsoft::WRL::ComPtr<IMFSample> sample;
	while (true)
	{
		hr = sourceReader->ReadSample(
			(DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			0, &streamIndex, &flags, &timestamp, &sample);

		if (FAILED(hr)) break;
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
		if (!sample) continue;

		// バッファを取得
		Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
		sample->ConvertToContiguousBuffer(&buffer);

		BYTE* audioData = nullptr;
		DWORD maxLength = 0, currentLength = 0;
		buffer->Lock(&audioData, &maxLength, &currentLength);

		pcmBuffer.insert(pcmBuffer.end(), audioData, audioData + currentLength);

		buffer->Unlock();
	}

	SoundData soundData{};
	soundData.wfex = *pWaveFormat;
	soundData.bufferSize = static_cast<UINT32>(pcmBuffer.size());
	soundData.pBuffer = new BYTE[soundData.bufferSize];
	memcpy(soundData.pBuffer, pcmBuffer.data(), soundData.bufferSize);

	CoTaskMemFree(pWaveFormat);
	return soundData;
}