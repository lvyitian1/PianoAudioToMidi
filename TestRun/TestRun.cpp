#include "stdafx.h"
#include "AudioLoader.h"
#include "FFmpegError.h"
//#include "DirectSound.h"
#include "AlignedVector.h"
#include "EnumTypes.h"
#include "ConstantQ.h"
#include "CqtError.h"
#include "HarmonicPercussive.h"

#include "KerasCnn.h"
#include "KerasError.h"

int main()
{
	using namespace std;

	shared_ptr<AudioLoader> song;
	try
	{
		song = make_shared<AudioLoader>(
			"C:/Users/Boris/Documents/Visual Studio 2017/Projects/Piano 2 Midi/Test songs/"
//			"chpn_op7_1MINp_align.wav"
			"MAPS_MUS-chpn-p7_SptkBGCl.wav"
//			"Eric Clapton - Wonderful Tonight.wma"
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

	assert(cqt->GetCQT().size() % cqt->GetNumBins() == 0 and "Constant-Q spectrum is not rectangular");
	const auto nSecs(cqt->GetCQT().size() / cqt->GetNumBins()
		* cqt->GetHopLength() / cqt->GetSampleRate());
	cout << "MIDI duration will be:\t" << nSecs / 60 << " min : " << nSecs % 60 << " sec" << endl;

	constexpr auto nFrames(7);

	vector<float> cqtHarm;
	vector<size_t> oPeaks;
#ifdef _DEBUG
	try {
#endif
		HarmonicPercussive hpss(cqt);
		cqtHarm.assign(hpss.GetHarmonic().size() + (nFrames / 2 * 2) * cqt->GetNumBins(), 0);
		const auto unusedIterFloat(copy(hpss.GetHarmonic().cbegin(), hpss.GetHarmonic().cend(),
			cqtHarm.begin() + (nFrames / 2) * static_cast<ptrdiff_t>(cqt->GetNumBins())));

		hpss.OnsetEnvelope();
		hpss.OnsetPeaksDetect();
		oPeaks = hpss.GetOnsetPeaks();

		hpss.Chromagram(false);
		hpss.ChromaSum();
		cout << "Key signature: maybe " << hpss.KeySignature() << endl;
#ifdef _DEBUG
	} catch (const CqtError& e)
	{
		cout << e.what() << endl;
		system("Pause");
		return 1;
	}
#endif

	vector<vector<float>> probabs;
	assert(cqtHarm.size() % cqt->GetNumBins() == 0 and "Harmonic spectrum is not rectangular");
	assert(cqtHarm.size() / cqt->GetNumBins() >= nFrames and
		"Padded spectrum must contain at least nFrames time frames");

	probabs.resize(cqtHarm.size() / cqt->GetNumBins() + 1 - nFrames);
	assert(probabs.size() >= oPeaks.back() and "Input and output durations do not match");
#ifdef _DEBUG
	for (auto& timeFrame : probabs)
	{
		timeFrame.resize(88);
		for (auto& p : timeFrame) p = .6f * rand() / RAND_MAX;
	}
#elif defined NDEBUG
	try
	{
		const KerasCnn model("KerasModel Dixon61 Frame54.json");
		cout << endl << model.GetLog() << endl;
		for (size_t i(0); i < probabs.size(); ++i)
		{
			probabs.at(i) = model.Predict2D(cqtHarm.data() +
				static_cast<ptrdiff_t>(i * cqt->GetNumBins()), 7, cqt->GetNumBins());
			cout << 100. * i * cqt->GetNumBins() / cqtHarm.size() << " %" << string(10, ' ') << '\r';
		}
		cout << 100 << "%" << string(10, ' ') << endl;
	}
	catch (const KerasError& e)
	{
		cout << e.what() << endl;
		system("Pause");
		return 1;
	}
#else
#pragma error Not debug, not release, then what is it?
#endif
	vector<vector<float>> result(oPeaks.size(), vector<float>(probabs.front().size()));
	for (size_t i(0); i < result.front().size(); ++i)
	{
		result.front().at(i) = max_element(probabs.cbegin(), result.size() == 1 ? probabs.cend()
			: (probabs.cbegin() + static_cast<ptrdiff_t>(oPeaks.front() + oPeaks.at(1)) / 2),
			[i](const vector<float>& lhs, const vector<float>& rhs)
		{ return lhs.at(i) < rhs.at(i); })->at(i);
		if (result.size() > 1)
		{
			result.back().at(i) = max_element(probabs.cbegin() + static_cast<ptrdiff_t>(
				*(oPeaks.cend() - 2) + oPeaks.back()) / 2, probabs.cend(),
				[i](const vector<float>& lhs, const vector<float>& rhs)
			{ return lhs.at(i) < rhs.at(i); })->at(i);

			for (size_t j(1); j < result.size() - 1; ++j) result.at(j).at(i) = max_element(
				probabs.cbegin() + static_cast<ptrdiff_t>(oPeaks.at(j - 1) + oPeaks.at(j)) / 2,
				probabs.cbegin() + static_cast<ptrdiff_t>(oPeaks.at(j) + oPeaks.at(j + 1)) / 2,
				[i](const vector<float>& lhs, const vector<float>& rhs)
			{ return lhs.at(i) < rhs.at(i); })->at(i);
		}
	}

	vector<vector<size_t>> notes(result.size());
	vector<pair<int, string>> gamma(12);
	for (size_t i(0); i < gamma.size(); ++i) gamma.at(i) = make_pair(0, vector<string>{
		"A", "Bb", "B", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab" }.at(i));

	for (size_t j(0); j < result.size(); ++j) for (size_t i(0); i < result.at(j).size(); ++i)
		if (result.at(j).at(i) > .5)
		{
			notes.at(j).emplace_back(i);
			++gamma.at(i % gamma.size()).first;
		}
	// TODO: remove
		else result.at(j).at(i) = 0;

	sort(gamma.rbegin(), gamma.rend());
	gamma.resize(7);
	for (const auto& n : gamma) cout << n.second << ' '; cout << endl;

	vector<string> blacks;
	for (const auto& n : gamma) if (n.second.length() > 1) blacks.emplace_back(n.second);
	sort(blacks.begin(), blacks.end());
	for (const auto& b : blacks) cout << b << ' '; cout << endl;

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

		class NumPy
		{
		public:
			NumPy() : data(np::empty(p::make_tuple(1, 1), np::dtype::get_builtin<float>())) {}
			explicit NumPy(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		auto main_namespace = p::import("__main__").attr("__dict__");
		main_namespace["CppStructCQT"] = p::class_<NumPy>("CppStructCQT").add_property(
			"data", p::make_getter(&NumPy::data));

		const auto pyScript(p::exec(R"(
from matplotlib      import pyplot as plt
from collections     import Counter
import numpy as np
import librosa as lbr
from   librosa.display     import specshow
from keras.models          import load_model
)", main_namespace));

		// 1 Load the song:
		const auto pyScript1(p::exec(R"(
#songName = '../../../Test songs/chpn_op7_1MINp_align.wav'
songName = '../../../Test songs/MAPS_MUS-chpn-p7_SptkBGCl.wav'
#songName = '../../../Test songs/Eric Clapton - Wonderful Tonight.wma'
#songName = '../../../Test songs/Chopin_op10_no3_p05_aligned.mp3'

nBins, nFrames = 4, 7

song = lbr.load(songName)[0]
song = lbr.effects.trim(song)[0]
songLen = int(lbr.get_duration(song))
print('Song duration\t{} min : {} sec'.format(songLen // 60, songLen % 60))
)", main_namespace));

		// 2 Constant-Q transform:
		NumPy pythonCQTharm(np::from_data(cqtHarm.data(), np::dtype::get_builtin<float>(),
				p::make_tuple(cqtHarm.size() / 88 / 4, 88 * 4),
				p::make_tuple(88 * 4 * sizeof cqtHarm.front(), sizeof cqtHarm.front()), p::object())),
			pythonOpeaks(np::from_data(oPeaks.data(), np::dtype::get_builtin<size_t>(),
				p::make_tuple(oPeaks.size()), p::make_tuple(sizeof oPeaks.front()), p::object()));
		main_namespace["cppCQTharm"] = p::ptr(&pythonCQTharm);
		main_namespace["cppOpeaks"] = p::ptr(&pythonOpeaks);

		const auto pyScript2(p::exec(R"(
cqtsHarm, cqtsPerc = lbr.decompose.hpss(lbr.amplitude_to_db(lbr.magphase(lbr.cqt(song,
	fmin=lbr.note_to_hz('A0'), n_bins=88*nBins, bins_per_octave=12*nBins))[0], ref=np.min))
print(cqtsHarm.shape[1], 'frames,', end='\t')
print('Cqts decibels in range [{:.0f} - {:.0f} - {:.0f}]'.format(cqtsHarm.min(), cqtsHarm.mean(), cqtsHarm.max()))
assert 0 <= cqtsHarm.min() < 15 and 20 < cqtsHarm.mean() < 45 and 70 < cqtsHarm.max() < 100

cqtsHarmCpp = cppCQTharm.data[nFrames // 2 : -(nFrames // 2)].T
print(cqtsHarmCpp.shape[1], 'frames,', end='\t')
print('Cqts decibels in range [{:.0f} - {:.0f} - {:.0f}]'.format(cqtsHarmCpp.min(), cqtsHarmCpp.mean(), cqtsHarmCpp.max()))
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
plt.show()

o_env = lbr.onset.onset_strength(S=cqtsPerc)
frames, times = lbr.onset.onset_detect(onset_envelope=o_env), lbr.frames_to_time(range(len(o_env)))
framesCpp, timesCpp = cppOpeaks.data, lbr.frames_to_time(range(cqtsHarmCpp.shape[1]))
)", main_namespace));

		// 4. Key Signature Estimation, Option 1.
		const auto pyScript4(p::exec(R"(
chroma = lbr.feature.chroma_cqt(C=lbr.db_to_amplitude(cqtsHarm - cqtsHarm.max()),
	fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)
chroma = chroma[:, frames].sum(1)

major = [np.corrcoef(chroma, np.roll([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88], i))[0, 1] for i in range(12)]
minor = [np.corrcoef(chroma, np.roll([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17], i))[0, 1] for i in range(12)]

keySignature = (['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'][
    major.index(max(major)) if max(major) > max(minor) else minor.index(max(minor)) - 3]
                + ('m' if max(major) < max(minor) else ''))
print(keySignature)
)", main_namespace));

		// 5. Pre-Processing of Spectrogram Data.
		const auto pyScript5(p::exec(R"(
cqts = cqtsHarm.T
cqts = np.vstack([np.zeros((nFrames // 2, cqtsHarm.shape[0]), cqtsHarm.dtype), cqtsHarm.T,
	np.zeros((nFrames // 2, cqtsHarm.shape[0]), cqtsHarm.dtype)])
cqts = np.array([cqts[range(f, f + nFrames)] for f in range(len(cqts) - nFrames + 1)])
print('Cqts grouped.')
)", main_namespace));

		// 6. Load CNN-Model and Predict Probabilities.
		vector<float> probabsFlat(probabs.size() * probabs.front().size());
		for (size_t i(0); i < probabs.size(); ++i) const auto unusedIter(copy(probabs.at(i).cbegin(),
			probabs.at(i).cend(), probabsFlat.begin() + static_cast<ptrdiff_t>(i * probabs.at(i).size())));
		NumPy pythonProba(np::from_data(probabsFlat.data(), np::dtype::get_builtin<float>(),
			p::make_tuple(probabs.size(), probabs.front().size()),
			p::make_tuple(probabs.front().size() * sizeof probabsFlat.front(),
				sizeof probabsFlat.front()), p::object()));
		main_namespace["cppProba"] = p::ptr(&pythonProba);

		const auto pyScript6(p::exec(R"(
model = load_model('Model Dixon61 Frame54.hdf5', compile = False) # HDF5 file
yProb = model.predict(cqts, verbose = 1)
yProbCpp = cppProba.data
)", main_namespace));

		// 7. Keep Only Notes with Probabilities >= some threshold.
		vector<float> resultFlat(result.size() * result.front().size());
		for (size_t i(0); i < result.size(); ++i) const auto unusedIter(copy(result.at(i).cbegin(),
			result.at(i).cend(), resultFlat.begin() + static_cast<ptrdiff_t>(i * result.at(i).size())));
		NumPy pythonPred(np::from_data(resultFlat.data(), np::dtype::get_builtin<float>(),
			p::make_tuple(result.size(), result.front().size()),
			p::make_tuple(result.front().size() * sizeof resultFlat.front(),
				sizeof resultFlat.front()), p::object()));
		main_namespace["cppPred"] = p::ptr(&pythonPred);

		const auto pyScript7(p::exec(R"(
yPred = yProb.max(0)
if len(frames) > 1:
    yPred = yProb[:(frames[0] + frames[1]) // 2].max(0)
    for i, fr in enumerate(frames[1:-1]):
        yPred = np.vstack([yPred, yProb[(frames[i] + fr) // 2 : (fr + frames[i + 2]) // 2].max(0)])
    yPred = np.vstack([yPred, yProb[(frames[-2] + frames[-1]) // 2 :].max(0)])
yPred[yPred < 0.5] = 0

plt.figure(figsize=(16, 8))
ax1 = plt.subplot(2, 1, 1)
plt.title('Python predicted notes')
specshow(cqtsHarm + cqtsPerc, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)
for t, n in zip(lbr.frames_to_time(frames), yPred):
    if t > 20: break
    plt.scatter([t] * np.count_nonzero(n), lbr.midi_to_hz(np.flatnonzero(n) + 21), c='w')

yPredCpp = cppPred.data
ax2 = plt.subplot(2, 1, 2)
plt.title('C++ predicted notes')
specshow(cqtsHarmCpp, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)
for t, n in zip(lbr.frames_to_time(framesCpp), yPredCpp):
    if t > 20: break
    plt.scatter([t] * np.count_nonzero(n), lbr.midi_to_hz(np.flatnonzero(n) + 21), c='w')

for a in[ax1, ax2]: a.set_xlim(0, 20)
plt.show()


notes = Counter()
for row in yPred: notes.update(Counter(np.argwhere(row).ravel() % 12))
gamma = [n for _, n in sorted([(count, ['A', 'Bb', 'B', 'C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab'][i])
                for i, count in notes.items()], reverse=True)[:7]]
blacks = sorted(n for n in gamma if len(n) > 1)
print(blacks, gamma)
)", main_namespace));

		// 8. Key Signature Estimation, Option 2.
		const auto pyScript8(p::exec(R"(
MajorMinor = lambda mj, mn: mj if gamma.index(mj) < gamma.index(mn) else mn + 'm'

if len(blacks) == 0: keySignature = MajorMinor('C', 'A')

elif len(blacks) == 1:
    if blacks[0] == 'F#':
        assert 'F' not in gamma
        keySignature = MajorMinor('G', 'E')
    elif blacks[0] == 'Bb':
        assert 'B' not in gamma
        keySignature = MajorMinor('F', 'D')
    else: assert False

elif len(blacks) == 2:
    if blacks == ['C#', 'F#']:
        assert 'C' not in gamma and 'F' not in gamma
        keySignature = MajorMinor('D', 'B')
    elif blacks == ['Bb', 'Eb']:
        assert 'B' not in gamma and 'E' not in gamma
        keySignature = MajorMinor('Bb', 'G')
    else: assert False

elif len(blacks) == 3:
    if blacks == ['Ab', 'C#', 'F#']:
        assert 'C' not in gamma and 'F' not in gamma and 'G' not in gamma
        keySignature = MajorMinor('A', 'F#')
    elif blacks == ['Ab', 'Bb', 'Eb']:
        assert 'B' not in gamma and 'E' not in gamma and 'A' not in gamma
        keySignature = MajorMinor('Eb', 'C')
    else: assert False

elif len(blacks) == 4:
    if blacks == ['Ab', 'C#', 'Eb', 'F#']:
        assert 'C' not in gamma and 'D' not in gamma and 'F' not in gamma and 'G' not in gamma
        keySignature = MajorMinor('E', 'C#')
    elif blacks == ['Ab', 'Bb', 'C#', 'Eb']:
        assert 'B' not in gamma and 'E' not in gamma and 'A' not in gamma and 'D' not in gamma
        keySignature = MajorMinor('Ab', 'F')
    else: assert False

elif 'B' in gamma and 'E' in gamma: keySignature = MajorMinor('B', 'Ab')
elif 'C' in gamma and 'F' in gamma: keySignature = MajorMinor('C#', 'Bb')
else: assert False

print(keySignature)
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