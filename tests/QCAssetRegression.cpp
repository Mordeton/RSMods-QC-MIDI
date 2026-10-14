#define QC_ASSET_TEST
#include "../DLL/Mods/ToneSourceAssetLoader.cpp"
#include "../DLL/Mods/ToneSourceTimeline.cpp"
#include <stdexcept>
#include <cmath>
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        std::string error; std::vector<uint8_t> decoded;
        std::vector<uint8_t> compressed = {120,156,99,96,100,98,102,97,101,99,231,224,228,226,230,225,229,227,23,16,20,18,22,17,21,19,151,144,148,146,150,145,149,147,87,80,84,82,86,81,85,83,215,208,212,210,214,209,213,211,55,48,52,50,54,49,53,51,183,176,180,178,182,177,181,179,7,0,170,224,7,225};
        std::vector<uint8_t> payload = {64,0,0,0}; payload.insert(payload.end(), compressed.begin(), compressed.end());
        std::vector<uint8_t> encrypted; std::array<uint8_t,16> iv{};
        Check(AesCtrTransform(kSngKeyPc, iv, payload, encrypted, error), "synthetic encryption failed");
        std::vector<uint8_t> entry(24); entry[0]=0x4a; entry[4]=1;
        entry.insert(entry.end(), encrypted.begin(), encrypted.end());
        Check(DecodeSngEntry(entry, decoded, error) && decoded.size()==64, "encrypted compressed fixture failed");
        for (int i=0;i<64;++i) Check(decoded[i]==i, "decoded bytes differ");
        entry.resize(24); entry.insert(entry.end(), payload.begin(), payload.end());
        Check(DecodeSngEntry(entry, decoded, error) && decoded.size()==64, "plain compressed fixture failed");
        entry[0]=0; Check(!DecodeSngEntry(entry, decoded, error), "invalid magic accepted");
        entry.resize(12); Check(!DecodeSngEntry(entry, decoded, error), "truncated header accepted");
        entry.assign(24,0);entry[0]=0x4a;entry[4]=1;
        payload={255,255,255,127};Check(AesCtrTransform(kSngKeyPc,iv,payload,encrypted,error),"encrypt oversized");
        entry.insert(entry.end(),encrypted.begin(),encrypted.end());
        Check(!DecodeSngEntry(entry,decoded,error),"oversized accepted");
        std::vector<QCAutomation::ToneSourceTimeline::ToneEvent> events; size_t ignored=0;
        Check(!ParseToneEventsFromDecodedSng({0xff,0xff,0xff,0xff},events,ignored,error),"malformed counts accepted");
        std::cout << "PASS: production SNG decoder synthetic and malformed fixtures\n";
        if (argc > 1) {
            _putenv_s("QC_TEST_GAME_ROOT", argv[1]);
            RebuildPsarcFileListIfNeeded();
            for (const auto& file : cachedPsarcFileList) Check(!EndsWithCaseInsensitive(file.string(),"_m.psarc"),"Mac archive eligible");
            using namespace QCAutomation::ToneSourceAssetLoader;
            ManifestArrangementDescriptor d; std::string matched;
            Check(ResolveDescriptorForCurrentSong("CarsJust","68D12AFF3F4C77ED71B13799FB6092A2",d,matched,error),error.c_str());
            Check(EndsWithCaseInsensitive(d.psarcPath.string(),"carsjust_p.psarc"),"wrong Cars platform selected");
            ArrangementToneTimeline timeline;
            Check(BuildTimelineFromDescriptor(d,timeline,error),error.c_str());
            Check(timeline.toneEvents.size()==3,"Cars event count differs");
            const float times[]={119.83f,136.06f,215.36f};
            const char* names[]={"CarsJust_solo","CarsJust_drive","CarsJust_synth"};
            for (size_t i=0;i<3;++i) {
                Check(std::abs(timeline.toneEvents[i].timeSec-times[i])<0.02f,"Cars event time differs");
                Check(timeline.toneEvents[i].authoredToneName==names[i],"Cars event name differs");
            }
            PsarcReader mac; Check(mac.Open(std::filesystem::path(argv[1])/"dlc/carsjust_m.psarc",error),error.c_str());
            Check(!FindSngEntryIndex(mac,d.sngPath),"Mac filename fallback accepted");
            std::cout << "PASS: installed Cars Windows selection and drive/solo/synth timeline; Mac fallback rejected\n";
            for (const char* song : {"TheStrokes1251", "IcedEarth-1776"}) {
                auto descriptors=GetDescriptorsForSong(song);Check(!descriptors.empty(),"known song metadata missing");
                for (const auto& item:descriptors) if (!item.sngPath.empty() && !item.toneBase.empty()) Check(BuildTimelineFromDescriptor(item,timeline,error),error.c_str());
                std::cout << "PASS: installed prior song " << song << "\n";
            }
        }
        return 0;
    } catch (const std::exception& ex) { std::cerr << "FAIL: " << ex.what() << '\n'; return 1; }
}
