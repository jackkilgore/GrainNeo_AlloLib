//make a load_dir
// git pull
// git submodule update --init --recursive
  // in KARLS MAT240 repo, this will update the allolib repo within it 
//make a randomizer when playback pos is idol 
// conenct QuNeo to paramters along with a GUI 
  //can send feedback back to QUNEO LEDS???? 
//ask karl how to output speaker separation 
//use type int Parameter 
  //ParameterInt 
    //ABOVE IS HOW YOU DO THIS 

#include "Gamma/SoundFile.h"
using namespace gam;

#include "al/core.hpp"
#include "al/util/imgui/al_Imgui.hpp"
#include "al/util/ui/al_ControlGUI.hpp"
#include "al/util/ui/al_Parameter.hpp"
using namespace al;
using namespace osc;

#include "synths.h"
using namespace diy;

#include <vector>
using namespace std;




const int SAMPLE_RATE = 44100;
const int BLOCK_SIZE = 512;
const int OUTPUT_CHANNELS = 2;
const int INPUT_CHANNELS = 2;

struct FloatPair {
  float l, r;
};

struct Granulator {

  void load(string fileName){
    SearchPaths searchPaths; 
    searchPaths.addSearchPath("..");

    string filePath = searchPaths.find(fileName).filepath();
    SoundFile soundFile;
    soundFile.path(filePath);
    if(!soundFile.openRead()){
      cout << "We could not read " << fileName << "!" << endl;
      exit(1);
    }

    Array *buffer = new Array();
    buffer->size = soundFile.frames()*soundFile.channels();
    buffer->data = new float[buffer->size];
    soundFile.read(buffer->data,buffer->size);
    this->soundClip.push_back(buffer);
      
    soundFile.close();
  }

  //keep some sound clips in memory 
  vector<Array*> soundClip;

  struct Grain{ 
    Array* source = nullptr;
    Line index;
    AttackDecay envelope;
    bool active = false;
    float pan;

    float operator()(){
      float f = envelope() * source->get(index());

      if(index.done()) active = false;

      return f;
    }
  };

  //storing grains 
  vector<Grain> grain;

  Granulator(){
    //arbitrary fixed number for how many grains we will allocate 
    grain.resize(1000); //wont create more than 1000 grains 
  }

  int activeGrainCount = 0;
///quneo/upButton/0/note_velocity
  //tweakable parameters 
    ParameterInt whichClip{"note_velocity", "upButton/0", 0, "quneo", 0, 8};
    //ParameterInt whichClip{"note_velocity", "downButton/0", 0, "quneo", 0, 8};
    //(0, source.size())
    ParameterInt grainDuration{"width", "longSlider", 60, "quneo", 0, 127};
    ParameterInt startPosition{"location", "longSlider", 60, "quneo", 0, 127};
    Parameter peakPosition{"/envelope", "", 0.1, "", 0.0, 1.0};
    Parameter amplitudePeak{"/amplitude", "", 0.707, "", 0.0, 1.0};
    Parameter panPosition{"/pan", "", 0.5, "", 0.0, 1.0};
    ParameterInt playbackRate{"location", "vSliders/3", 0.0, "quneo", 0, 127};
    Parameter birthRate{"/frequency", "", 55, "", 0, 200};
    ParameterInt PositionRandRange{"location", "hSliders/3", 0, "quneo", 0, 127};


  //this governs the rate at which grains are created 
  Edge grainBirth; 

  //this method makes a new grain out of a dead/inactive one. 
  // 
  void recycle(Grain& g) {
    // choose which sound clip this grain pulls from
    g.source = soundClip[whichClip];
    // startTime and endTime are in units of sample
    float startTime = g.source->size * startPosition/127.0 * rnd::uniform(1.0,(PositionRandRange/127.0 + 1.1));
    float endTime =
        startTime + (grainDuration/127.0) * ::SAMPLE_RATE * powf(2.0, playbackRate/127.0);

    g.index.set(startTime, endTime, (grainDuration/127.0));

    // riseTime and fallTime are in units of second
    float riseTime = (grainDuration/127.0) * peakPosition;
    float fallTime = ((grainDuration/127.0)) - riseTime;
    g.envelope.set(riseTime, fallTime, amplitudePeak);

    g.pan = panPosition;

    // permit this grain to sound!
    g.active = true;
  }

  //make the next sample 
  FloatPair operator()() {
    // figure out if we should generate (recycle) more grains; then do so.
    //
    grainBirth.frequency(birthRate);
    if (grainBirth()) {
      for (Grain& g : grain)
        if (!g.active) {
          recycle(g);
          break;
        }
    }

    // figure out which grains are active. for each active grain, get the next
    // sample; sum all these up and return that sum.
    //
    float left = 0, right = 0;
    activeGrainCount = 0;
    for (Grain& g : grain)
      if (g.active) {
        activeGrainCount++;
        float f = g();
        left += f * (1 - g.pan);
        right += f * g.pan;
      }
    return {left, right};
  }

};




struct MyApp : public App {
   bool show_gui = true;
   float background = 0.21;
   Granulator granulator;
   ControlGUI gui;
   PresetHandler presetHandler{"GranulatorPresets"};
   PresetServer presetServer{"0.0.0.0", 9011};
   Recv server;

  void onCreate() override {
    parameterServer().print();
    
    granulator.load("0.wav");
    granulator.load("1.wav");
    granulator.load("2.wav");
    granulator.load("3.wav");
    granulator.load("4.wav");
    granulator.load("panlinespoonDrop.aiff");

   // server.open(9020,"localhost",0.05);
    //server.handler(*this);
    //server.start();
    //ParameterServer() << Y;
    parameterServer().addListener("127.0.0.1", 9020);

    gui.init();
    gui << granulator.whichClip << granulator.grainDuration
        << granulator.startPosition << granulator.peakPosition
        << granulator.amplitudePeak << granulator.panPosition
        << granulator.playbackRate << granulator.birthRate 
        << granulator.PositionRandRange;

    parameterServer() << granulator.whichClip << granulator.grainDuration
                      << granulator.startPosition << granulator.peakPosition
                      << granulator.amplitudePeak << granulator.panPosition
                      << granulator.playbackRate << granulator.birthRate
                      << granulator.PositionRandRange;
    
  }

  void onAnimate(double dt) override {
    // pass show_gui for use_input param to turn off interactions
    // when not showing gui
    navControl().active(!gui.usingInput());
  }

  void onDraw(Graphics& g) override {
    g.clear(background);
    gui.draw(g);
  }

  void onSound(AudioIOData& io) override {
    while (io()) {
      FloatPair p = granulator();
      io.out(0) = p.l;
      io.out(1) = p.r;
    }
  }

  void onKeyDown(const Keyboard& k) override {
    if (k.key() == 'g') {
      show_gui = !show_gui;
    }
  }

//look at interaction tutorial by Andres
  // PARAMETER class 
    //look into how to fit the below name space of QuNeo into 
    // Parameter p {"name", "group", 0.0, "prefix", -1.0f, 1.0f}
  void onMessage(osc::Message& m) override {
    m.print();
    if(m.addressPattern() == "/quneo/vSliders/3/location")
    {
		int val;
		m >> val;
    float v = (val/127.0f)*3;
    if(v < 0.0001) v = 0.0001;
      //cout << "vSlider3 Location: "<< v << endl;
      //granulator.grainDuration = v;

    }
    if(m.addressPattern() == "/quneo/longSlider/width")
    {
      int val;
      m >> val;
      cout << "longSlider Width: "<< val << endl;
    }
    if(m.addressPattern() == "/quneo/pads/3/drum/pressure")
    {
      int val;
      m >> val;
      cout << "drumPad Pressure: "<< val << endl;
    }

  }

  void onExit() override { shutdownIMGUI(); }
};


int main() {
  MyApp app;
  app.initAudio(::SAMPLE_RATE, ::BLOCK_SIZE, ::OUTPUT_CHANNELS, ::INPUT_CHANNELS);
  app.start();
  return 0;
}







 