#include "stdafx.h"
#include "AudioLoader.h"
//#include "DirectSound.h"
#include "ShortTimeFourier.h"
#include "CqtBasis.h"
#include "CqtError.h"

int main()
{
	using namespace std;

	vector<float> rawSong;
	try
	{
		const AudioLoader song("C:/Users/Boris/Documents/Visual Studio 2017/Projects/Piano 2 Midi/Test songs/"
//			"chpn_op7_1MINp_align.wav"
//			"MAPS_MUS-chpn-p7_SptkBGCl.wav"
			"Eric Clapton - Wonderful Tonight.wma"
//			"Chopin_op10_no3_p05_aligned.mp3"
		);
		cout << "Format:\t\t" << song.GetFormatName() << endl
			<< "Audio Codec:\t" << song.GetCodecName() << endl
			<< "Bit_rate:\t" << song.GetBitRate() << endl;

		song.Decode();
		cout << "Duration:\t" << song.GetNumSeconds() / 60 << " min : "
			<< song.GetNumSeconds() % 60 << " sec" << endl;

		song.MonoResample();
		const auto iter(rawSong.insert(rawSong.cend(), reinterpret_cast<const float*>(song.GetRawData().data()),
			reinterpret_cast<const float*>(song.GetRawData().data() + song.GetRawData().size())));

//		DirectSound sound;
//		song.MonoResample(0, AV_SAMPLE_FMT_S16);
//		WAVEFORMATEX wavHeader{ WAVE_FORMAT_PCM, 1, 22'050, 44'100, 2, 16 };
//		const auto wavStart(song.GetRawData().data());
//		const auto wavSize(song.GetRawData().size());
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

	ShortTimeFourier stft(rawSong);
	const auto stftData(stft.RealForward());

	unique_ptr<CqtBasis> qBasis;
	try
	{
		qBasis = make_unique<CqtBasis>(22'050, 27.5f, 88, 12, 1);
		qBasis->Calculate(.01f);
	}
	catch (const CqtError& e)
	{
		cout << e.what() << endl;
		system("Pause");
		return 1;
	}

	for (const auto& fftFilt : qBasis->GetCqtFilters())
	{
		float minReal((numeric_limits<float>::max)()), minImag((numeric_limits<float>::max)()),
			maxReal((numeric_limits<float>::min)()), maxImag((numeric_limits<float>::min)());
		complex<float> sumFilt;
		for (const auto& val : fftFilt)
		{
			minReal = min(minReal, val.real());
			minImag = min(minImag, val.imag());
			maxReal = max(maxReal, val.real());
			maxImag = max(maxImag, val.imag());
			sumFilt += val;
		}
		cout << minReal << ' ' << minImag << ' ' << maxReal << ' ' << maxImag << ' ' << sumFilt << endl;
	}

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

		class Song
		{
		public:
			Song() : data(np::empty(p::tuple(1), np::dtype::get_builtin<float>())) {}
			explicit Song(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructSong"] = p::class_<Song>("CppStructSong").add_property(
			"data", p::make_getter(&Song::data));
		Song pythonSong(np::from_data(rawSong.data(), np::dtype::get_builtin<float>(),
			p::make_tuple(rawSong.size()), p::make_tuple(sizeof rawSong.front()), p::object()));
		main_namespace["cppSong"] = p::ptr(&pythonSong);
		
		// 1 Load the song:
		const auto pyScript1(p::exec(R"(
print(cppSong.data.shape, cppSong.data.min(), cppSong.data.mean(), cppSong.data.max())

import librosa as lbr

#songName = '../../../Test songs/chpn_op7_1MINp_align.wav'
songName = '../../../Test songs/Eric Clapton - Wonderful Tonight.wma'
#songName = '../../../Test songs/Chopin_op10_no3_p05_aligned.mp3'

harmMargin, nBins, nFrames = 4, 5, 5

#song = lbr.effects.trim(lbr.load(songName)[0])[0]
song = lbr.load(songName)[0]
print(song.shape, song.min(), song.mean(), song.max())
songLen = int(lbr.get_duration(song))
print('Song duration\t{} min : {} sec'.format(songLen // 60, songLen % 60))

from matplotlib import pyplot as plt

plt.figure(figsize=(16, 8))

ax1 = plt.subplot(2, 1, 1)
plt.title('Cpp WaveForm')
plt.plot(range(200_000), cppSong.data[1_000_000:1_200_000])

ax2 = plt.subplot(2, 1, 2)
plt.title('Python WaveForm')
plt.plot(range(200_000), song[1_000_000:1_200_000])

plt.show()
)", main_namespace));

		class CQbasis
		{
		public:
			CQbasis() : data(np::empty(p::make_tuple(1, 1), np::dtype::get_builtin<complex<float>>())) {}
			explicit CQbasis(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructCQbasis"] = p::class_<CQbasis>("CppStructCQbasis").add_property(
			"data", p::make_getter(&CQbasis::data));

		vector<complex<float>> qBasisData(qBasis->GetCqtFilters().size()
			* qBasis->GetCqtFilters().front().size());
		for (size_t i(0); i < qBasis->GetCqtFilters().size(); ++i) const auto unusedIter(copy(
			qBasis->GetCqtFilters().at(i).cbegin(), qBasis->GetCqtFilters().at(i).cend(),
			qBasisData.begin() + i * qBasis->GetCqtFilters().front().size()));

		CQbasis pythonCQbasis(np::from_data(qBasisData.data(), np::dtype::get_builtin<complex<float>>(),
			p::make_tuple(qBasis->GetCqtFilters().size(), qBasis->GetCqtFilters().front().size()),
			p::make_tuple(qBasis->GetCqtFilters().front().size() * sizeof qBasisData.front(),
				sizeof qBasis->GetCqtFilters().front().front()), p::object()));
		main_namespace["cppCQbasis"] = p::ptr(&pythonCQbasis);

		const auto pyScript1_2(p::exec(R"(
plt.plot(cppCQbasis.data[-1][2_500:3_700].real)
plt.show()

plt.plot(cppCQbasis.data[-1][2_500:3_700].imag)
plt.show()

'''from librosa.display import specshow
import numpy as np

#Plot one octave of filters in time and frequency
basis = cppCQbasis.data

plt.figure(figsize=(16, 4))

plt.subplot(1, 2, 1)
for i, f in enumerate(basis[:12]):
    f_scale = lbr.util.normalize(f) / 2
    plt.plot(i + f_scale.real)
    plt.plot(i + f_scale.imag, linestyle=':')

notes = lbr.midi_to_note(np.arange(24, 24 + len(basis)))

plt.yticks(np.arange(12), notes[:12])
plt.ylabel('CQ filters')
plt.title('CQ filters (one octave, time domain)')
plt.xlabel('Time (samples at 22050 Hz)')
plt.legend(['Real', 'Imaginary'])

plt.subplot(1, 2, 2)
F = np.abs(np.fft.fftn(basis, axes=[-1]))
# Keep only the positive frequencies
F = F[:, :(1 + F.shape[1] // 2)]

specshow(F, x_axis='linear')
plt.yticks(np.arange(len(notes))[::12], notes[::12])
plt.ylabel('CQ filters')
plt.title('CQ filter magnitudes (frequency domain)')'''

plt.show()
)", main_namespace));

		class STFT
		{
		public:
			STFT() : data(np::empty(p::make_tuple(1, 1), np::dtype::get_builtin<complex<float>>())) {}
			explicit STFT(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructSTFT"] = p::class_<STFT>("CppStructSTFT").add_property(
			"data", p::make_getter(&STFT::data));

		vector<complex<float>> stftDataFlat(stftData.size() * stftData.front().size());
		for (size_t i(0); i < stftData.size(); ++i) const auto unusedIter(copy(
			stftData.at(i).cbegin(), stftData.at(i).cend(),
			stftDataFlat.begin() + i * stftData.front().size()));

		STFT pythonSTFT(np::from_data(stftDataFlat.data(), np::dtype::get_builtin<complex<float>>(),
			p::make_tuple(stftData.size(), stftData.front().size()),
			p::make_tuple(stftData.front().size() * sizeof stftDataFlat.front(),
				sizeof stftData.front().front()), p::object()));
		main_namespace["cppSTFT"] = p::ptr(&pythonSTFT);

		const auto pyScript1_3(p::exec(R"(
from librosa.display import specshow

print(cppSTFT.data.shape, cppSTFT.data.min(), cppSTFT.data.mean(), cppSTFT.data.max())
stft = lbr.stft(song)
print(stft.shape, stft.min(), stft.mean(), stft.max())

plt.figure(figsize=(16, 8))

ax1 = plt.subplot(2, 1, 1)
plt.title('Cpp Power spectrogram')
specshow(lbr.amplitude_to_db(lbr.magphase(cppSTFT.data.T)[0]), y_axis='log', x_axis='time')

ax2 = plt.subplot(2, 1, 2)
plt.title('Python Power spectrogram')
specshow(lbr.amplitude_to_db(lbr.magphase(stft)[0]), y_axis='log', x_axis='time')

plt.show()
)", main_namespace));

		system("Pause");
		return 0;

		// 2 CQT-transform:
		const auto pyScript2(p::exec(R"(
import numpy as np

songHarm = lbr.effects.harmonic(song, margin=harmMargin)
cqts = lbr.util.normalize(lbr.amplitude_to_db(lbr.magphase(lbr.cqt(
    songHarm, fmin=lbr.note_to_hz('A0'), n_bins=88*nBins, bins_per_octave=12*nBins))[0], ref=np.min))
print(cqts.shape[1], 'frames,', end='\t')
#assert cqts.min() > 0 and cqts.max() == 1 and len(np.hstack([(c == 1).nonzero()[0] for c in cqts.T])) == cqts.shape[1]
print('normalized cqts in range [{:.2e} - {:.3f} - {:.3f}]'.format(cqts.min(), cqts.mean(), cqts.max()))

# Not using row-wise batch norm of each cqt-bin, even though:
maxBins = cqts.max(1)
assert maxBins.shape == (440,)
print('All maximums should be ones, but there are only {:.1%} ones and min of max is {:.3f}'.format(
    maxBins[maxBins == 1].sum() / len(maxBins), maxBins.min()))

print('And now the same for Cpp data:')
songHarmCpp = lbr.effects.harmonic(cppSong.data, margin=harmMargin)
cqtsCpp = lbr.util.normalize(lbr.amplitude_to_db(lbr.magphase(lbr.cqt(
    songHarmCpp, fmin=lbr.note_to_hz('A0'), n_bins=88*nBins, bins_per_octave=12*nBins))[0], ref=np.min))
print(cqtsCpp.shape[1], 'frames,', end='\t')
print(len(np.hstack([(c == 1).nonzero()[0] for c in cqtsCpp.T])), 'ones')
assert cqtsCpp.min() > 0 and cqtsCpp.max() == 1 # and len(np.hstack([(c == 1).nonzero()[0] for c in cqtsCpp.T])) == cqts.shape[1]
print('normalized cqts in range [{:.2e} - {:.3f} - {:.3f}]'.format(cqtsCpp.min(), cqtsCpp.mean(), cqtsCpp.max()))

# Not using row-wise batch norm of each cqt-bin, even though:
maxBins = cqtsCpp.max(1)
assert maxBins.shape == (440,)
print('All maximums should be ones, but there are only {:.1%} ones and min of max is {:.3f}'.format(
    maxBins[maxBins == 1].sum() / len(maxBins), maxBins.min()))
)", main_namespace));

		// 3 Note onsets:
		const auto pyScript3(p::exec(R"(
plt.figure(figsize=(16, 8))
ax1 = plt.subplot(2, 1, 1)
plt.title('Cpp Power spectrogram')
specshow(cqtsCpp, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

ax2 = plt.subplot(2, 1, 2)
plt.title('Python Power spectrogram')
specshow(cqts, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

#o_env, frames = lbr.onset.onset_strength(song), lbr.onset.onset_detect(song)
'''
times = lbr.frames_to_time(range(len(o_env)))
plt.vlines(times[frames], lbr.note_to_hz('A0'), lbr.note_to_hz('C8'), color='w', alpha=.8)

ax3 = plt.subplot(3, 1, 3)
plt.plot(times, o_env, label='Onset strength')
plt.vlines(times[frames], 0, o_env.max(), color='r', label='Onsets')
plt.legend()
plt.ylim(0, o_env.max())
'''
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