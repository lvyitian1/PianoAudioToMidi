#include "stdafx.h"
#include "AudioLoader.h"
#include "FFmpegError.h"
//#include "DirectSound.h"
#include "AlignedVector.h"
#include "ConstantQ.h"
#include "CqtError.h"

#include "HarmonicPercussive.h"

int main()
{
	using namespace std;

	shared_ptr<AudioLoader> song;
	try
	{
		song = make_shared<AudioLoader>(
			"C:/Users/Boris/Documents/Visual Studio 2017/Projects/Piano 2 Midi/Test songs/"
//			"chpn_op7_1MINp_align.wav"
//			"MAPS_MUS-chpn-p7_SptkBGCl.wav"
			"Eric Clapton - Wonderful Tonight.wma"
//			"Chopin_op10_no3_p05_aligned.mp3"
		);
		cout << "Format:\t\t" << song->GetFormatName() << endl
			<< "Audio Codec:\t" << song->GetCodecName() << endl
			<< "Bit_rate:\t" << song->GetBitRate() << endl;

		song->Decode();
		cout << "Duration:\t" << song->GetNumSeconds() / 60 << " min : "
			<< song->GetNumSeconds() % 60 << " sec" << endl;

		song->MonoResample();

//		DirectSound sound;
//		song->MonoResample(0, false);
//		WAVEFORMATEX wavHeader{ WAVE_FORMAT_PCM, 1, 22'050, 44'100, 2, 16 };
//		const auto wavStart(song->GetRawData());
//		const auto wavSize(song->GetNumBytes());
//		sound.Play(&wavHeader, wavStart, wavSize);
//		system("Pause");
	}
	catch (const FFmpegError& e)
	{
		cout << e.what() << endl;
		system("Pause");
		return 1;
	}
//	catch (const SoundError& e) { cout << e.what() << endl; }

	shared_ptr<ConstantQ> cqt;
	try
	{
		if (song->GetBytesPerSample() == sizeof(uint16_t)) song->MonoResample(0);
		cqt = make_shared<ConstantQ>(song, 88 * 4, 12 * 4);
	}
	catch (const CqtError& e)
	{
		cout << e.what() << endl;
		system("Pause");
		return 1;
	}

	cqt->Amplitude2power();
	cqt->TrimSilence();
	cqt->Power2db(*min_element(cqt->GetCQT().cbegin(), cqt->GetCQT().cend()));

	vector<float> cqtHarm, cqtPerc, oEnv;
	vector<size_t> oPeaks;
#ifdef _DEBUG
	try {
#endif
		HarmonicPercussive hpss(cqt);
		cqtHarm = hpss.GetHarmonic();
		cqtPerc = hpss.GetPercussive();

		hpss.OnsetEnvelope();
		hpss.OnsetPeaksDetect();
		oEnv = hpss.GetOnsetEnvelope();
		oPeaks = hpss.GetOnsetPeaks();
#ifdef _DEBUG
	} catch (const CqtError& e)
	{
		cout << e.what() << endl;
		system("Pause");
		return 1;
	}
#endif

#ifdef NDEBUG
	namespace p = boost::python;
	namespace np = p::numpy;
	try
	{
		Py_SetPath(L"C:/Anaconda3/Lib");
		Py_Initialize();

		const auto pathAppend(p::import("sys").attr("path").attr("append"));
		auto pythonLib(pathAppend("C:/Anaconda3/Lib/site-packages"));
		pythonLib = pathAppend("C:/Anaconda3/DLLs");
		np::initialize();

		auto main_namespace = p::import("__main__").attr("__dict__");

		const auto pyScript(p::exec(R"(
from matplotlib      import pyplot as plt

from collections     import Counter

import numpy as np
#import mido
import librosa as lbr
from   librosa.display     import specshow

#from keras.models          import load_model
#from keras.utils.vis_utils import model_to_dot
)", main_namespace));

		// 1 Load the song:
		const auto pyScript1(p::exec(R"(
#songName = '../../../Test songs/chpn_op7_1MINp_align.wav'
#songName = '../../../Test songs/MAPS_MUS-chpn-p7_SptkBGCl.wav'
songName = '../../../Test songs/Eric Clapton - Wonderful Tonight.wma'
#songName = '../../../Test songs/Chopin_op10_no3_p05_aligned.mp3'

nBins, nFrames = 4, 7

song = lbr.load(songName)[0]
song = lbr.effects.trim(song)[0]
songLen = int(lbr.get_duration(song))
print('Song duration\t{} min : {} sec'.format(songLen // 60, songLen % 60))
)", main_namespace));

		// 2 Constant-Q transform:
		class CQT
		{
		public:
			CQT() : data(np::empty(p::make_tuple(1, 1), np::dtype::get_builtin<float>())) {}
			explicit CQT(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructCQT"] = p::class_<CQT>("CppStructCQT").add_property(
			"data", p::make_getter(&CQT::data));

		CQT pythonCQTharm(np::from_data(cqtHarm.data(), np::dtype::get_builtin<float>(),
				p::make_tuple(cqtHarm.size() / 88 / 4, 88 * 4),
				p::make_tuple(88 * 4 * sizeof cqtHarm.front(), sizeof cqtHarm.front()), p::object())),
			pythonCQTperc(np::from_data(cqtPerc.data(), np::dtype::get_builtin<float>(),
				p::make_tuple(cqtPerc.size() / 88 / 4, 88 * 4),
				p::make_tuple(88 * 4 * sizeof cqtPerc.front(), sizeof cqtPerc.front()), p::object())),
			pythonOenv(np::from_data(oEnv.data(), np::dtype::get_builtin<float>(),
				p::make_tuple(oEnv.size()), p::make_tuple(sizeof oEnv.front()), p::object())),
			pythonOpeaks(np::from_data(oPeaks.data(), np::dtype::get_builtin<size_t>(),
				p::make_tuple(oPeaks.size()), p::make_tuple(sizeof oPeaks.front()), p::object()));
		main_namespace["cppCQTharm"] = p::ptr(&pythonCQTharm);
		main_namespace["cppCQTperc"] = p::ptr(&pythonCQTperc);
		main_namespace["cppOenv"] = p::ptr(&pythonOenv);
		main_namespace["cppOpeaks"] = p::ptr(&pythonOpeaks);

		const auto pyScript2(p::exec(R"(
cqtsHarm, cqtsPerc = lbr.decompose.hpss(lbr.amplitude_to_db(lbr.magphase(lbr.cqt(song,
	fmin=lbr.note_to_hz('A0'), n_bins=88*nBins, bins_per_octave=12*nBins))[0], ref=np.min))

print(cqtsHarm.shape[1], 'frames,', end='\t')
print('Cqts decibels in range [{:.0f} - {:.0f} - {:.0f}]'.format(cqtsHarm.min(), cqtsHarm.mean(), cqtsHarm.max()))
assert 0 <= cqtsHarm.min() < 15 and 20 < cqtsHarm.mean() < 45 and 70 < cqtsHarm.max() < 100

print('And now the same for C++ data...')
cqtsHarmCpp, cqtsPercCpp = cppCQTharm.data.T, cppCQTperc.data.T

print(cqtsHarmCpp.shape[1], 'frames,', end='\t')
print('Cqts decibels in range [{:.0f} - {:.0f} - {:.0f}]'.format(cqtsHarmCpp.min(), cqtsHarmCpp.mean(), cqtsHarmCpp.max()))
assert 0 <= cqtsHarmCpp.min() < 15 and 20 < cqtsHarmCpp.mean() < 45 and 70 < cqtsHarmCpp.max() < 100
)", main_namespace));

		// 3 Note onsets:
		const auto pyScript3(p::exec(R"(
plt.figure(figsize=(16, 8))
ax1 = plt.subplot(2, 1, 1)
plt.title('Python Harmonic power spectrogram')
specshow(cqtsHarm, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

ax2 = plt.subplot(2, 1, 2)
plt.title('C++ Harmonic power spectrogram')
specshow(cqtsHarmCpp, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

for a in [ax1, ax2]: a.set_xlim(0, 10)
plt.show()


plt.figure(figsize=(16, 8))
ax1 = plt.subplot(2, 1, 1)
plt.title('Python Percussive power spectrogram')
specshow(cqtsPerc, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

ax2 = plt.subplot(2, 1, 2)
plt.title('C++ Percussive power spectrogram')
specshow(cqtsPercCpp, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

for a in [ax1, ax2]: a.set_xlim(0, 10)
plt.show()


o_env = lbr.onset.onset_strength(S=cqtsPerc)
frames, times = lbr.onset.onset_detect(onset_envelope=o_env), lbr.frames_to_time(range(len(o_env)))

o_envCpp = cppOenv.data
framesCpp, timesCpp = cppOpeaks.data, lbr.frames_to_time(range(len(o_envCpp)))

plt.figure(figsize=(16, 8))
ax1 = plt.subplot(2, 1, 1)
plt.title('Python Onset envelope')
plt.plot(times, o_env, label='Onset strength')
plt.vlines(times[frames], 0, o_env.max(), color='r', label='Onsets')
plt.legend()
plt.ylim(0, o_env.max())

ax2 = plt.subplot(2, 1, 2)
plt.title('C++ Onset envelope')
plt.plot(timesCpp, o_envCpp, label='Onset strength')
plt.vlines(timesCpp[framesCpp], 0, o_envCpp.max(), color='r', label='Onsets')
plt.legend()
plt.ylim(0, o_envCpp.max())

for a in [ax1, ax2]: a.set_xlim(0, 10)
plt.show()
)", main_namespace));
	}
	catch (const p::error_already_set&)
	{
		if (PyErr_ExceptionMatches(PyExc_AssertionError)) cout << "Python assertion failed" << endl;
		PyErr_Print();
	}
#endif

	system("Pause");
}