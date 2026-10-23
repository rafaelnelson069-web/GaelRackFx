#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr int knobCount = 11;

enum Knob
{
    input = 0,
    mix,
    output,
    eq,
    fet,
    opto,
    satur,
    multiband,
    clear,
    air,
    afro
};

constexpr std::array<const char*, knobCount> parameterIds
{
    "input", "mix", "output", "eq", "fet", "opto",
    "satur", "mb", "clear", "air", "afro"
};

constexpr std::array<const char*, knobCount> parameterNames
{
    "Input", "Mix", "Output", "EQ", "1176", "2A",
    "Satur", "MB", "Clear", "Air", "Afro"
};

constexpr std::array<float, knobCount> defaults
{
    0.0f, 100.0f, -2.0f, 24.0f, 20.0f, 25.0f,
    10.0f, 15.0f, 12.0f, 15.0f, 0.0f
};

float toGain(float db)
{
    return juce::Decibels::decibelsToGain(db);
}

float toDb(float gain)
{
    return juce::Decibels::gainToDecibels(
        juce::jmax(gain, 1.0e-9f), -180.0f);
}

float coefficient(double sampleRate, double milliseconds)
{
    return static_cast<float>(
        std::exp(-1.0 / (sampleRate * milliseconds * 0.001)));
}

void follow(float& state, float value, float attack, float release)
{
    const float c = value > state ? attack : release;
    state = value + c * (state - value);
}

float compressionGain(
    float level, float threshold, float ratio, float knee)
{
    const float over = toDb(level) - threshold;
    const float slope = 1.0f - 1.0f / ratio;
    float reduction = 0.0f;

if (over >= knee * 0.5f)
    {
        reduction = slope * over;
    }
    else if (over > -knee * 0.5f)
    {
        const float x = over + knee * 0.5f;
        reduction = slope * x * x / (2.0f * knee);
    }

return toGain(-reduction);
}

struct LowPass
{
    float pole = 0.0f;
    float state = 0.0f;

void prepare(double sr, float frequency)
    {
        frequency = juce::jmin(
            frequency, static_cast<float>(sr * 0.4));

pole = std::exp(
            -juce::MathConstants<float>::twoPi
            * frequency / static_cast<float>(sr));

state = 0.0f;
    }

float process(float x)
    {
        state = (1.0f - pole) * x + pole * state;
        return state;
    }

void reset()
    {
        state = 0.0f;
    }
};

struct BandPass
{
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

float z1 = 0.0f;
    float z2 = 0.0f;

void prepare(double sr, float frequency, float q)
    {
        frequency = juce::jmin(
            frequency, static_cast<float>(sr * 0.40));

const float w =
            juce::MathConstants<float>::twoPi
            * frequency / static_cast<float>(sr);

const float alpha = std::sin(w) / (2.0f * q);
        const float inverseA0 = 1.0f / (1.0f + alpha);

b0 = alpha * inverseA0;
        b1 = 0.0f;
        b2 = -alpha * inverseA0;
        a1 = -2.0f * std::cos(w) * inverseA0;
        a2 = (1.0f - alpha) * inverseA0;

reset();
    }

float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;

return y;
    }

void reset()
    {
        z1 = z2 = 0.0f;
    }
};

struct Preset
{
    const char* name;
    std::array<float, knobCount> values;
};

// Ordre : Input, Mix, Output, EQ, 1176, 2A,
// Satur, MB, Clear, Air, Afro.
//
// Ces valeurs sont des points de depart.
// Elles ne remplacent pas un reglage adapte a la prise.
const std::array<Preset, 6> presets
{{
    {
        "Femme",
        { 0, 100, -2, 28, 20, 32, 9, 20, 12, 18, 0 }
    },
    {
        "Homme",
        { 0, 100, -2, 32, 25, 35, 14, 25, 15, 24, 0 }
    },
    {
        "Trap",
        { 0, 100, -3, 35, 48, 40, 32, 35, 22, 28, 12 }
    },
    {
        "Afrobeat",
        { 0, 100, -3, 26, 30, 32, 20, 26, 15, 28, 38 }
    },
    {
        "Salegy",
        { 0, 100, -3, 30, 38, 34, 24, 28, 15, 30, 22 }
    },
    {
        "Slow",
        { 0, 100, -2, 22, 15, 30, 10, 15, 10, 20, 25 }
    }
}};
}

class GaelRackProcessor final : public juce::AudioProcessor
{
public:
    GaelRackProcessor()
        : AudioProcessor(
            BusesProperties()
                .withInput(
                    "Input", juce::AudioChannelSet::stereo(), true)
                .withOutput(
                    "Output", juce::AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "GaelRackState", createLayout())
    {
        for (int i = 0; i < knobCount; ++i)
        {
            raw[static_cast<size_t>(i)] =
                state.getRawParameterValue(
                    parameterIds[static_cast<size_t>(i)]);
        }

rawMic = state.getRawParameterValue("mic");
    }

const juce::String getName() const override
    {
        return juce::String::fromUTF8("GaëlRack Fx");
    }

bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

double getTailLengthSeconds() const override { return 4.0; }

int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}

const juce::String getProgramName(int) override
    {
        return "Default";
    }

void changeProgramName(int, const juce::String&) override {}

bool isBusesLayoutSupported(
        const BusesLayout& layouts) const override
    {
        const auto in = layouts.getMainInputChannelSet();
        const auto out = layouts.getMainOutputChannelSet();

return in == out
            && (in == juce::AudioChannelSet::mono()
                || in == juce::AudioChannelSet::stereo());
    }

void prepareToPlay(double newSampleRate, int) override
    {
        sampleRate = newSampleRate > 0.0
            ? newSampleRate : 44100.0;

constexpr std::array<float, 6> frequencies
        {
            220.0f, 450.0f, 900.0f,
            1800.0f, 3600.0f, 6500.0f
        };

for (int ch = 0; ch < 2; ++ch)
        {
            for (int b = 0; b < 6; ++b)
            {
                resonance[ch][b].prepare(
                    sampleRate,
                    frequencies[static_cast<size_t>(b)],
                    3.2f);
            }

micLow[ch].prepare(sampleRate, 160.0f);
            micHigh[ch].prepare(sampleRate, 4500.0f);

splitLow[ch].prepare(sampleRate, 250.0f);
            splitHigh[ch].prepare(sampleRate, 4000.0f);

airLow[ch].prepare(sampleRate, 7500.0f);

delay[ch].assign(
                static_cast<size_t>(
                    std::ceil(sampleRate * 1.2)) + 4,
                0.0f);
        }

for (auto& s : smooth)
            s.reset(sampleRate, 0.030);

micLowGain.reset(sampleRate, 0.050);
        micHighGain.reset(sampleRate, 0.050);

delayTime.reset(sampleRate, 0.150);
        delayTime.setCurrentAndTargetValue(
            static_cast<float>(sampleRate * 0.250));

eqEnvelopeC = coefficient(sampleRate, 28.0);
        eqGainC = coefficient(sampleRate, 12.0);

fetAttack = coefficient(sampleRate, 0.25);
        fetRelease = coefficient(sampleRate, 60.0);

optoAttack = coefficient(sampleRate, 12.0);
        optoRelease = coefficient(sampleRate, 180.0);

mbAttack = coefficient(sampleRate, 8.0);
        mbRelease = coefficient(sampleRate, 100.0);

clearAttack = coefficient(sampleRate, 4.0);
        clearRelease = coefficient(sampleRate, 90.0);

clearClose = coefficient(sampleRate, 35.0);
        clearOpen = coefficient(sampleRate, 5.0);

updateTargets(true);
        reset();
    }

void releaseResources() override {}

void reset() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& filter : resonance[ch])
                filter.reset();

micLow[ch].reset();
            micHigh[ch].reset();
            splitLow[ch].reset();
            splitHigh[ch].reset();
            airLow[ch].reset();

std::fill(delay[ch].begin(), delay[ch].end(), 0.0f);
        }

fullPower = 0.0f;
        bandPower.fill(0.0f);
        bandReduction.fill(0.0f);

fetEnvelope = 0.0f;
        optoPower = 0.0f;
        mbEnvelope.fill(0.0f);

clearEnvelope = 0.0f;
        clearGain = 1.0f;

writePosition = 0;
    }

void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        midi.clear();

const int channels =
            juce::jmin(2, getTotalNumInputChannels());

for (int ch = channels; ch < buffer.getNumChannels(); ++ch)
            buffer.clear(ch, 0, buffer.getNumSamples());

if (channels == 0 || delay[0].empty())
            return;

updateTargets(false);

double bpm = 120.0;

if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                if (const auto hostBpm = position->getBpm())
                {
                    if (std::isfinite(*hostBpm) && *hostBpm > 0.0)
                        bpm = *hostBpm;
                }
            }
        }

// Croche = une demi-noire.
        const double seconds =
            juce::jlimit(0.09, 0.90, 30.0 / bpm);

delayTime.setTargetValue(
            static_cast<float>(seconds * sampleRate));

float* left = buffer.getWritePointer(0);
        float* right = channels == 2
            ? buffer.getWritePointer(1) : nullptr;

for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            std::array<float, knobCount> p {};

for (int k = 0; k < knobCount; ++k)
                p[k] = smooth[k].getNextValue();

const float lowCorrection = micLowGain.getNextValue();
            const float highCorrection = micHighGain.getNextValue();

float dry[2] { left[n], right != nullptr ? right[n] : 0.0f };
            float x[2] {};

for (int ch = 0; ch < channels; ++ch)
            {
                if (!std::isfinite(dry[ch]))
                    dry[ch] = 0.0f;

x[ch] = dry[ch] * p[input];

// Protection contre des donnees d'entree aberrantes.
                x[ch] = juce::jlimit(-32.0f, 32.0f, x[ch]);

const float low = micLow[ch].process(x[ch]);
                const float high =
                    x[ch] - micHigh[ch].process(x[ch]);

x[ch] += low * (lowCorrection - 1.0f)
                       + high * (highCorrection - 1.0f);
            }

// EQ adaptatif :
            // recherche de concentration d'energie dans six bandes.
            // Ce n'est pas un modele spectral de type soothe2.
            float bands[2][6] {};
            float instantaneousPower = 0.0f;

for (int ch = 0; ch < channels; ++ch)
            {
                instantaneousPower += x[ch] * x[ch];

for (int b = 0; b < 6; ++b)
                    bands[ch][b] = resonance[ch][b].process(x[ch]);
            }

instantaneousPower /= static_cast<float>(channels);

fullPower =
                instantaneousPower
                + eqEnvelopeC * (fullPower - instantaneousPower);

for (int b = 0; b < 6; ++b)
            {
                float energy = 0.0f;

for (int ch = 0; ch < channels; ++ch)
                    energy += bands[ch][b] * bands[ch][b];

energy /= static_cast<float>(channels);

bandPower[b] =
                    energy + eqEnvelopeC * (bandPower[b] - energy);

const float concentration =
                    bandPower[b] / (fullPower + 1.0e-10f);

const float excess = juce::jlimit(
                    0.0f, 1.0f,
                    (concentration - 0.18f) / 0.55f);

const float target = 0.65f * p[eq] * excess;

bandReduction[b] =
                    target + eqGainC * (bandReduction[b] - target);
            }

for (int ch = 0; ch < channels; ++ch)
            {
                for (int b = 0; b < 6; ++b)
                    x[ch] -= bands[ch][b] * bandReduction[b];
            }

// Compresseur rapide, detecteur stereo lie.
            float peak = 0.0f;

for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(x[ch]));

follow(fetEnvelope, peak, fetAttack, fetRelease);

const float fastThreshold = -6.0f - 6.0f * p[fet];
            const float fastRatio = 1.0f + 7.0f * p[fet];

const float fastGain = compressionGain(
                fetEnvelope, fastThreshold, fastRatio, 4.0f);

for (int ch = 0; ch < channels; ++ch)
                x[ch] *= fastGain;

// Compresseur lent RMS.
            float power = 0.0f;

for (int ch = 0; ch < channels; ++ch)
                power += x[ch] * x[ch];

power /= static_cast<float>(channels);

follow(optoPower, power, optoAttack, optoRelease);

const float slowGain = compressionGain(
                std::sqrt(juce::jmax(0.0f, optoPower)),
                -22.0f,
                1.0f + 2.5f * p[opto],
                8.0f);

for (int ch = 0; ch < channels; ++ch)
                x[ch] *= slowGain;

// Trois bandes a reconstruction additive.
            // A MB=0, leur somme redonne exactement le signal.
            float split[2][3] {};

for (int ch = 0; ch < channels; ++ch)
            {
                const float low = splitLow[ch].process(x[ch]);
                const float lowAndMid = splitHigh[ch].process(x[ch]);

split[ch][0] = low;
                split[ch][1] = lowAndMid - low;
                split[ch][2] = x[ch] - lowAndMid;
            }

constexpr float thresholds[3]
            {
                -20.0f, -22.0f, -25.0f
            };

float mbGains[3] {};

for (int b = 0; b < 3; ++b)
            {
                float bandPeak = 0.0f;

for (int ch = 0; ch < channels; ++ch)
                {
                    bandPeak = juce::jmax(
                        bandPeak, std::abs(split[ch][b]));
                }

follow(
                    mbEnvelope[b], bandPeak, mbAttack, mbRelease);

mbGains[b] = compressionGain(
                    mbEnvelope[b],
                    thresholds[b],
                    1.0f + 1.8f * p[multiband],
                    6.0f);
            }

for (int ch = 0; ch < channels; ++ch)
            {
                x[ch] = 0.0f;

for (int b = 0; b < 3; ++b)
                    x[ch] += split[ch][b] * mbGains[b];

// Saturation douce.
                // Pas de sur-echantillonnage dans cette version.
                const float drive = 1.0f + 4.0f * p[satur];

const float coloured =
                    std::tanh(x[ch] * drive)
                    / std::sqrt(drive);

x[ch] += p[satur] * 0.60f * (coloured - x[ch]);
            }

// Clear : expansion descendante moderee.
            peak = 0.0f;

for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(x[ch]));

follow(
                clearEnvelope, peak, clearAttack, clearRelease);

const float belowThreshold =
                juce::jmin(0.0f, toDb(clearEnvelope) + 42.0f);

const float expansionDb = juce::jlimit(
                -12.0f, 0.0f,
                belowThreshold * 0.6f) * p[clear];

const float desiredClearGain = toGain(expansionDb);

const float gainC = desiredClearGain < clearGain
                ? clearClose : clearOpen;

clearGain =
                desiredClearGain
                + gainC * (clearGain - desiredClearGain);

for (int ch = 0; ch < channels; ++ch)
            {
                x[ch] *= clearGain;

const float high =
                    x[ch] - airLow[ch].process(x[ch]);

x[ch] += high * p[air] * 0.65f;
            }

// Echo Afro.
            // La boucle de retard est vide lorsque Afro reste a zero.
            const float baseDelay = delayTime.getNextValue();

for (int ch = 0; ch < channels; ++ch)
            {
                const float stereoOffset =
                    channels == 2 && ch == 1 ? 1.013f : 1.0f;

const float echo =
                    readDelay(ch, baseDelay * stereoOffset);

delay[ch][writePosition] =
                    x[ch] * p[afro] + echo * 0.18f;

x[ch] += echo * 0.28f * p[afro];
            }

++writePosition;

if (writePosition >= static_cast<int>(delay[0].size()))
                writePosition = 0;

for (int ch = 0; ch < channels; ++ch)
            {
                // Mix lineaire : pas de hausse de 3 dB au centre.
                // Le signal sec est pris avant Input.
                float result =
                    (dry[ch] * (1.0f - p[mix])
                     + x[ch] * p[mix]) * p[output];

if (!std::isfinite(result))
                    result = 0.0f;

if (ch == 0)
                    left[n] = result;
                else
                    right[n] = result;
            }
        }
    }

bool hasEditor() const override { return true; }

juce::AudioProcessorEditor* createEditor() override;

void getStateInformation(juce::MemoryBlock& destination) override
    {
        const auto snapshot = state.copyState();

if (auto xml = snapshot.createXml())
            copyXmlToBinary(*xml, destination);
    }

void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size))
        {
            if (xml->hasTagName(state.state.getType().toString()))
            {
                const auto restored =
                    juce::ValueTree::fromXml(*xml);

if (restored.isValid())
                    state.replaceState(restored);
            }
        }
    }

void loadPreset(int index)
    {
        if (index < 0 || index >= static_cast<int>(presets.size()))
            return;

// Le preset ne remplace pas le micro choisi.
        const auto& values = presets[static_cast<size_t>(index)].values;

for (int k = 0; k < knobCount; ++k)
        {
            if (auto* parameter =
                    state.getParameter(parameterIds[k]))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(values[k]));
                parameter->endChangeGesture();
            }
        }
    }

juce::AudioProcessorValueTreeState state;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout
    createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

for (int k = 0; k < knobCount; ++k)
        {
            const bool isGain = k == input || k == output;

const float minimum = isGain ? -24.0f : 0.0f;
            const float maximum = k == input ? 24.0f
                : (k == output ? 12.0f : 100.0f);

const auto attributes =
                juce::AudioParameterFloatAttributes()
                    .withLabel(isGain ? "dB" : "%");

layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(parameterIds[k], 1),
                parameterNames[k],
                juce::NormalisableRange<float>(
                    minimum, maximum, 0.1f),
                defaults[k],
                attributes));
        }

layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("mic", 1),
            "Mic",
            juce::StringArray
            {
                "AKG P120",
                "AT2020",
                "BM800"
            },
            0));

return layout;
    }

void updateTargets(bool initialise)
    {
        for (int k = 0; k < knobCount; ++k)
        {
            const float value = raw[k]->load();

const float target = k == input || k == output
                ? toGain(value) : value * 0.01f;

if (initialise)
                smooth[k].setCurrentAndTargetValue(target);
            else
                smooth[k].setTargetValue(target);
        }

const int selectedMic = juce::jlimit(
            0, 2, static_cast<int>(std::lround(rawMic->load())));

// Corrections artistiques indicatives, NON mesurees.
        // Elles ne modelisent ni une capsule, ni une lampe,
        // ni un transformateur, ni un microphone de reference.
        constexpr float lowDb[3]  { -1.0f, -1.5f, -2.0f };
        constexpr float highDb[3] {  0.8f, -0.7f, -1.5f };

const float lowTarget = toGain(lowDb[selectedMic]);
        const float highTarget = toGain(highDb[selectedMic]);

if (initialise)
        {
            micLowGain.setCurrentAndTargetValue(lowTarget);
            micHighGain.setCurrentAndTargetValue(highTarget);
        }
        else
        {
            micLowGain.setTargetValue(lowTarget);
            micHighGain.setTargetValue(highTarget);
        }
    }

float readDelay(int channel, float delaySamples) const
    {
        const int size = static_cast<int>(delay[channel].size());

delaySamples = juce::jlimit(
            1.0f, static_cast<float>(size - 2), delaySamples);

float position =
            static_cast<float>(writePosition) - delaySamples;

if (position < 0.0f)
            position += static_cast<float>(size);

const int first = static_cast<int>(position);
        const int second = first + 1 < size ? first + 1 : 0;

const float fraction = position - static_cast<float>(first);

return delay[channel][first]
            + fraction
            * (delay[channel][second] - delay[channel][first]);
    }

double sampleRate = 44100.0;

std::array<std::atomic<float>*, knobCount> raw {};
    std::atomic<float>* rawMic = nullptr;

std::array<juce::SmoothedValue<float>, knobCount> smooth;
    juce::SmoothedValue<float> micLowGain;
    juce::SmoothedValue<float> micHighGain;
    juce::SmoothedValue<float> delayTime;

BandPass resonance[2][6];
    LowPass micLow[2];
    LowPass micHigh[2];
    LowPass splitLow[2];
    LowPass splitHigh[2];
    LowPass airLow[2];

std::vector<float> delay[2];
    int writePosition = 0;

float fullPower = 0.0f;
    std::array<float, 6> bandPower {};
    std::array<float, 6> bandReduction {};

float fetEnvelope = 0.0f;
    float optoPower = 0.0f;

std::array<float, 3> mbEnvelope {};

float clearEnvelope = 0.0f;
    float clearGain = 1.0f;

float eqEnvelopeC = 0.0f;
    float eqGainC = 0.0f;

float fetAttack = 0.0f;
    float fetRelease = 0.0f;
    float optoAttack = 0.0f;
    float optoRelease = 0.0f;

float mbAttack = 0.0f;
    float mbRelease = 0.0f;

float clearAttack = 0.0f;
    float clearRelease = 0.0f;
    float clearClose = 0.0f;
    float clearOpen = 0.0f;

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GaelRackProcessor)
};

class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel()
    {
        setColour(
            juce::Slider::textBoxTextColourId,
            juce::Colour(0xfff4eee1));

setColour(
            juce::Slider::textBoxBackgroundColourId,
            juce::Colour(0xff102c33));

setColour(
            juce::Slider::textBoxOutlineColourId,
            juce::Colours::transparentBlack);

setColour(
            juce::ComboBox::backgroundColourId,
            juce::Colour(0xff15353c));

setColour(
            juce::ComboBox::textColourId,
            juce::Colour(0xfff4eee1));

setColour(
            juce::ComboBox::outlineColourId,
            juce::Colour(0xff55726a));

setColour(
            juce::PopupMenu::backgroundColourId,
            juce::Colour(0xff102c33));

setColour(
            juce::PopupMenu::textColourId,
            juce::Colour(0xfff4eee1));

setColour(
            juce::PopupMenu::highlightedBackgroundColourId,
            juce::Colour(0xff287b6b));
    }

void drawRotarySlider(
        juce::Graphics& g,
        int x, int y, int width, int height,
        float position,
        float startAngle, float endAngle,
        juce::Slider&) override
    {
        const auto bounds =
            juce::Rectangle<float>(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(width),
                static_cast<float>(height)).reduced(9.0f);

const auto centre = bounds.getCentre();

const float radius =
            juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

const float angle =
            startAngle + position * (endAngle - startAngle);

juce::Path backgroundArc;
        backgroundArc.addCentredArc(
            centre.x, centre.y,
            radius, radius,
            0.0f, startAngle, endAngle, true);

g.setColour(juce::Colour(0xff34525a));
        g.strokePath(backgroundArc, juce::PathStrokeType(5.0f));

juce::Path valueArc;
        valueArc.addCentredArc(
            centre.x, centre.y,
            radius, radius,
            0.0f, startAngle, angle, true);

g.setColour(juce::Colour(0xffebc878));
        g.strokePath(valueArc, juce::PathStrokeType(5.0f));

g.setColour(juce::Colour(0xff16343c));
        g.fillEllipse(
            centre.x - radius + 9.0f,
            centre.y - radius + 9.0f,
            (radius - 9.0f) * 2.0f,
            (radius - 9.0f) * 2.0f);

const float pointerLength = radius * 0.58f;

const juce::Point<float> tip
        {
            centre.x + std::sin(angle) * pointerLength,
            centre.y - std::cos(angle) * pointerLength
        };

g.setColour(juce::Colour(0xfff7edcf));
        g.drawLine(
            centre.x, centre.y, tip.x, tip.y, 3.0f);
    }
};

class GaelRackEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GaelRackEditor(GaelRackProcessor& processorToUse)
        : AudioProcessorEditor(&processorToUse),
          processor(processorToUse)
    {
        setLookAndFeel(&look);

for (int k = 0; k < knobCount; ++k)
        {
            auto& slider = knobs[k];

slider.setSliderStyle(
                juce::Slider::RotaryHorizontalVerticalDrag);

slider.setTextBoxStyle(
                juce::Slider::TextBoxBelow, false, 100, 22);

slider.setRotaryParameters(
                juce::MathConstants<float>::pi * 1.25f,
                juce::MathConstants<float>::pi * 2.75f,
                true);

slider.setTextValueSuffix(
                k == input || k == output ? " dB" : " %");

slider.setDoubleClickReturnValue(true, defaults[k]);
            slider.setName(parameterNames[k]);

addAndMakeVisible(slider);

attachments[k] = std::make_unique<SliderAttachment>(
                processor.state, parameterIds[k], slider);
        }

for (int i = 0; i < static_cast<int>(presets.size()); ++i)
            presetMenu.addItem(presets[i].name, i + 1);

presetMenu.setName("Preset");
        presetMenu.setTextWhenNothingSelected("Choisir un preset");

presetMenu.onChange = [this]
        {
            const int index = presetMenu.getSelectedId() - 1;

if (index >= 0)
            {
                processor.loadPreset(index);

// Menu d'action : les valeurs sont memorisees
                // par le plugin, sans afficher un nom devenu
                // inexact apres modification manuelle.
                presetMenu.setSelectedId(
                    0, juce::dontSendNotification);
            }
        };

micMenu.setName("Mic");
        micMenu.addItem("AKG P120", 1);
        micMenu.addItem("AT2020", 2);
        micMenu.addItem("BM800", 3);

addAndMakeVisible(presetMenu);
        addAndMakeVisible(micMenu);

micAttachment = std::make_unique<ComboAttachment>(
            processor.state, "mic", micMenu);

setResizable(false, false);
        setSize(1000, 700);
    }

~GaelRackEditor() override
    {
        setLookAndFeel(nullptr);
    }

void resized() override
    {
        const auto area =
            getLocalBounds().withTrimmedTop(126)
                            .withTrimmedBottom(58)
                            .reduced(26, 0);

const int cellWidth = area.getWidth() / 5;
        const int cellHeight = area.getHeight() / 3;

for (int k = 0; k < 13; ++k)
        {
            cards[k] = juce::Rectangle<int>(
                area.getX() + (k % 5) * cellWidth,
                area.getY() + (k / 5) * cellHeight,
                cellWidth,
                cellHeight).reduced(6);

if (k < knobCount)
            {
                knobs[k].setBounds(
                    cards[k].withTrimmedTop(28)
                            .withTrimmedBottom(8)
                            .reduced(13, 0));
            }
        }

presetMenu.setBounds(
            cards[11].getX() + 12,
            cards[11].getY() + 64,
            cards[11].getWidth() - 24,
            34);

micMenu.setBounds(
            cards[12].getX() + 12,
            cards[12].getY() + 64,
            cards[12].getWidth() - 24,
            34);
    }

void paint(juce::Graphics& g) override
    {
        juce::ColourGradient ocean(
            juce::Colour(0xff092b37), 0.0f, 0.0f,
            juce::Colour(0xff176356),
            static_cast<float>(getWidth()),
            static_cast<float>(getHeight()),
            false);

g.setGradientFill(ocean);
        g.fillAll();

drawMadagascar(g);

// Petit rappel des couleurs du drapeau malgache.
        g.setColour(juce::Colours::white);
        g.fillRect(34, 30, 15, 38);

g.setColour(juce::Colour(0xffef5148));
        g.fillRect(49, 30, 29, 19);

g.setColour(juce::Colour(0xff38a772));
        g.fillRect(49, 49, 29, 19);

g.setColour(juce::Colour(0xfff7efd8));
        g.setFont(juce::FontOptions(35.0f, juce::Font::bold));

g.drawText(
            juce::String::fromUTF8("GaëlRack Fx"),
            96, 23, 550, 52,
            juce::Justification::centredLeft);

g.setColour(juce::Colour(0xffc4dbd0));
        g.setFont(juce::FontOptions(14.0f));

g.drawText(
            "VOCAL RACK  /  MONO + STEREO  /  MADAGASCAR",
            36, 85, 740, 22,
            juce::Justification::centredLeft);

for (int k = 0; k < 13; ++k)
        {
            const auto bounds = cards[k].toFloat();

g.setColour(
                juce::Colour(0xff081e27).withAlpha(0.80f));
            g.fillRoundedRectangle(bounds, 13.0f);

g.setColour(
                juce::Colour(0xffbad3b0).withAlpha(0.19f));
            g.drawRoundedRectangle(bounds, 13.0f, 1.0f);

g.setColour(juce::Colour(0xfff1e7ce));
            g.setFont(juce::FontOptions(16.0f, juce::Font::bold));

const juce::String name = k < knobCount
                ? parameterNames[k] : (k == 11 ? "Preset" : "Mic");

g.drawText(
                name,
                cards[k].withHeight(30),
                juce::Justification::centred);
        }

g.setColour(juce::Colour(0xffbad3c8));
        g.setFont(juce::FontOptions(11.0f));

g.drawText(
            "Point de depart vocal",
            cards[11].withTrimmedTop(110).withHeight(22),
            juce::Justification::centred);

g.drawText(
            "Profil indicatif",
            cards[12].withTrimmedTop(110).withHeight(22),
            juce::Justification::centred);

g.setColour(juce::Colour(0xffc4dbd0));
        g.setFont(juce::FontOptions(12.0f));

g.drawText(
            "DSP adaptatif - aucun modele IA",
            34, getHeight() - 38, 430, 22,
            juce::Justification::centredLeft);

g.setColour(juce::Colour(0xfff0d18c));
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));

g.drawText(
            "PRESETS VOCAUX  /  DOUBLE-CLIC SUR UN POTENTIOMETRE : RESET",
            34, getHeight() - 59, getWidth() - 68, 18,
            juce::Justification::centredLeft);
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void drawMadagascar(juce::Graphics& g)
    {
        // Silhouette decorative stylisee, sans ressource externe.
        juce::Path island;
        island.startNewSubPath(842.0f, 26.0f);
        island.lineTo(858.0f, 35.0f);
        island.lineTo(864.0f, 48.0f);
        island.lineTo(857.0f, 61.0f);
        island.lineTo(852.0f, 78.0f);
        island.lineTo(841.0f, 91.0f);
        island.lineTo(835.0f, 108.0f);
        island.lineTo(824.0f, 119.0f);
        island.lineTo(817.0f, 106.0f);
        island.lineTo(821.0f, 91.0f);
        island.lineTo(815.0f, 77.0f);
        island.lineTo(822.0f, 63.0f);
        island.lineTo(825.0f, 48.0f);
        island.lineTo(834.0f, 39.0f);
        island.closeSubPath();
        g.setColour(juce::Colour(0xfff0d18c).withAlpha(0.16f));
        g.fillPath(island);
        g.setColour(juce::Colour(0xfff0d18c).withAlpha(0.42f));
        g.strokePath(island, juce::PathStrokeType(1.5f));
    }

    GaelRackProcessor& processor;
    RackLookAndFeel look;
    std::array<juce::Slider, knobCount> knobs;
    std::array<std::unique_ptr<SliderAttachment>, knobCount> attachments;
    juce::ComboBox presetMenu;
    juce::ComboBox micMenu;
    std::unique_ptr<ComboAttachment> micAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GaelRackEditor)
};

juce::AudioProcessorEditor* GaelRackProcessor::createEditor()
{
    return new GaelRackEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GaelRackProcessor();
}
