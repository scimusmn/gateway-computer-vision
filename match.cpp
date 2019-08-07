#include <opencv2/opencv.hpp>
#include <SerialStream.h>
#include <iostream>
#include <vector>
#include <math.h>
#include <chrono>    //benchmarking (can be removed for production)


using namespace std;
using namespace std::chrono;
using namespace cv;
using namespace LibSerial;


//class for benchmarking
class benchmark {
    time_point<steady_clock> start = steady_clock::now();
    string title;
public:
    benchmark(const string& title) : title(title) {}

    ~benchmark() {
        auto diff = steady_clock::now() - start;
        cout << title << " took " << duration <double, milli> (diff).count() << " ms" << endl;
    }
};

//store template mats and the associated names
typedef struct {
  vector<double> regions; // edge pixel count vector
  double region_avg;      // average edge pixel count
  string name;            // human-readable name for the sign
  string signal;            // signal to send to the arduino
} tmplt;

//store match region and level
typedef struct {
  Rect region;           // the ROI
  bool good;             // is this a "good" match?
} match;

//store matched name and confidence
typedef struct {
  string name;          // human-readable name of the matched template
  string signal;         // signal to send
  float match_level;     // confidence the match is correct
} type_match;

//function to find region with a sign
match get_region( Mat&, int, int, int, double, int );

//function to determine best template match to sign
type_match get_match( Mat, vector<tmplt>&, int, int, int, int, int);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  
int main(int argc, char** argv) {
  // ROI identification constants
  int canny_roi_low;            // canny low threshold for roi identification
  int canny_roi_high;           // canny high threshold for roi identification
  int canny_roi_kernel;         // canny kernel size for roi identification
  double circularity_threshold; // threshold for roi circularity
  int min_roi_area;             // minimum roi size

  // template matching constants
  int canny_match_low;          // canny low threshold for template matching
  int canny_match_high;         // canny high threshold for template matching
  int canny_match_kernel;       // canny kernel size
  int vertical_segments;        // vertical image divisions for template matching
  int horizontal_segments;      // horizontal image divisions
  			        
  vector<tmplt> templates;      // store all of the template images

  // misc other constants
  VideoCapture camera;          // the webcam
			        
  string window_name;           // the name of the display window

  string current_name;          // name to keep track of present sign
  int current_counts;           // keep track of how long the present sign has been detected
  int counts_for_signal;        // how long should the sign be seen before sending a signal?

  SerialStream arduino;         // the Arduino serial port

  //~~~~~~

  cout << "OpenCV version : " << CV_VERSION << endl;
  
  //load our settings
  FileStorage fs;
  fs.open("match-settings.xml", FileStorage::READ);
  {
    //    ~~~~~~~~    ROI id settings    ~~~~~~~~
    canny_roi_low  = (int) fs["canny-roi-low"];
    canny_roi_high = (int) fs["canny-roi-high"];
    canny_roi_kernel = (int) fs["canny-roi-kernel"];

    circularity_threshold = (double) fs["circularity-threshold"];

    min_roi_area = (int) fs["minimum-roi-area"];

    cout << "ROI identification settings:" << endl;
    cout << "  Circularity threshold: " << circularity_threshold << endl;
    cout << "  Canny interval: (" << canny_roi_low << ", " << canny_roi_high << ")" << endl;
    cout << "  Kernel size: " << canny_roi_kernel << endl;
    cout << "  Minimum area: " << min_roi_area << " pixels" << endl;
  }


  
  {
    //    ~~~~~~~~     template match settings    ~~~~~~~~     
    vertical_segments   = (int) fs["vertical-segments"];
    horizontal_segments = (int) fs["horizontal-segments"];

    canny_match_low  = (int) fs["canny-match-low"];
    canny_match_high = (int) fs["canny-match-high"];
    canny_match_kernel = (int) fs["canny-match-kernel"];

    cout << "Template match settings:" << endl;
    cout << "  Subdivision: " << horizontal_segments << "x" << vertical_segments << endl;
    cout << "  Canny interval: (" << canny_match_low << ", " << canny_match_high << ")" << endl;
    cout << "  Kernel size: " << canny_match_kernel << endl;
  }

  
  
  {
    //    ~~~~~~~~     load the match templates    ~~~~~~~~     
    cout << "Loading templates:" << endl;
    FileNode n = fs["templates"];
    FileNodeIterator it = n.begin(), it_end = n.end();
    for (; it != it_end; ++it) {
      tmplt new_template;
      new_template.region_avg = 0;

      new_template.name = (string) (*it)["name"];
      new_template.signal = (string) (*it)["signal"];
      string fname = (string) (*it)["image"];
      cout << "  " << new_template.name << ": " << fname << endl;

      Mat image = imread( fname, IMREAD_COLOR );
      if (image.empty()) {
	cout << "could not load file " << fname << endl;
	return -1;
      }

      cvtColor( image, image, COLOR_BGR2GRAY ); // convert to grayscale
      blur( image, image, Size(3,3) ); // blur for less noise
      Canny( image, image, canny_match_low, canny_match_high, canny_match_kernel );  // detect edges

      //get segments of the template image
      int dx = image.cols / horizontal_segments;
      int dy = image.rows / vertical_segments;
      for (int y=0; y<=image.rows-dy; y+=dy) {
	for (int x=0; x<=image.cols-dx; x+=dx) {
	  Mat segment ( image, Rect ( x, y, dx, dy ) );
	  int count = countNonZero( segment ); // count number of nonzero pixels
	  double fraction = (double) count / ( dx*dy ); // get fraction of nonzero pixels
	  new_template.regions.push_back( fraction ); // store in the template
	  new_template.region_avg += fraction;
	}
      }

      //compute region average
      new_template.region_avg /= new_template.regions.size();
      
      //save template
      templates.push_back( new_template );
    }
  }

  
  
  cout << "loaded " << templates.size() << " templates." << endl;

  counts_for_signal = (int) fs["counts-for-signal"];

  cout << "Frames for signal: " << counts_for_signal << endl;


  
  {
    //    ~~~~~~~~     open the camera    ~~~~~~~~     
    int cameranum = (int) fs["camera"];
    camera = VideoCapture(cameranum);
    if ( !camera.isOpened() ) {
      cerr << "ERROR: could not open camera " << cameranum << endl;
      return 0;
    }
    camera.set(CAP_PROP_BUFFERSIZE, 1); // buffer only 1 frame
    cout << "opened camera " << cameranum << endl;
  }

  {
    //    ~~~~~~~~    open the Arduino port    ~~~~~~~~
    string serial_port_name = (string) fs["serial-port"];
    arduino.Open( serial_port_name );
    arduino.SetBaudRate( SerialStreamBuf::BAUD_9600 );
    if ( !arduino.IsOpen() ) {
      // something went wrong, abort
      cerr << "ERROR: could not open " << serial_port_name << endl;
      return -1;
    }
    else {
      cout << "Opened Arduino on port " << serial_port_name << endl;
    }
  }

  window_name = (string) fs["window-name"];
  cout << "window name is " << window_name << endl;
  
  fs.release(); // done with the settings

  
  
  Mat frame;
  
  camera >> frame;
  cout << "image is " << frame.rows << "x" << frame.cols << endl;

  namedWindow(window_name);

  cout << "beginning main loop" << endl;

  

  //    ~~~~~~~~     main loop    ~~~~~~~~     
  
  while (true) {
    /* Steps in the loop:
         1. Get a frame from the camera
	 2. Find a region in which a sign is likely to be
	 3. Match that region to one of the sign templates
	 4. If that match has been held for enough frames, send a signal to the Arduino
	 5. Draw a rectangle and label the ROI on the image
	 6. Render
    */

    // STEP ONE: Get a frame from the camera
    camera >> frame;

    // STEP TWO: Find a region in which a sign is likely to be
    match m = get_region ( frame,
			   canny_roi_low, canny_roi_high, canny_roi_kernel,
			   circularity_threshold, min_roi_area );

    // STEP THREE: Match that region to one of the sign templates
    if ( m.good ) {
      // the match is acceptable
      // this is only untrue if there is no ROI detected
      
      Rect roi = m.region;
      // ensure that the ROI is wholly within the camera's view
      // ( if this is not true, trying to match to it will throw an error )
      if ( 0 <= roi.x && 0 <= roi.width && roi.x + roi.width <= frame.cols && 0 <= roi.y && 0 <= roi.height && roi.y + roi.height <= frame.rows ) {
	//the ROI is wholly within the camera's view

	// actually do the matching!
	Mat region ( frame, roi );
	type_match m2 = get_match( region, templates,
				 canny_match_low, canny_match_high, canny_match_kernel,
				 vertical_segments, horizontal_segments );

	// STEP FOUR: If that match has been held for enough frames, send a signal to the Arduino
	// first, make sure that we ~can~ actually ID the sign
	if ( m2.name != "unknown" ) {
	  // we know what sign it is!
	  if ( m2.name == current_name ) {
	    // the sign is the same one we've been looking at
	    // increment the frame counter
	    current_counts++;
	  }
	  else {
	    // a new sign ( or we something's gone wrong IDing the present one )
	    current_name = m2.name; // reset the name
	    current_counts = 0;  // reset the frame counter
	  }
	}

	// have we held the match for enough frames?
	if (current_counts == counts_for_signal) {
	  // yes!
	  // note that this only sends the signal once
	  // this is to avoid repeating a signal when the operator didn't intend to
	  cout << "sending signal for " << current_name << endl;
	  arduino << m2.signal << endl;
	}

	// STEP FIVE: Draw a rectangle and label the ROI on the image
	rectangle ( frame, m.region, Scalar(50), 3 );
	putText ( frame, m2.name, Point(m.region.x, m.region.y), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,0,255), 2);
      }
    }
    else {
      // no sign was detected, reset frame counter
      current_counts = 0;
    }

    // STEP SIX: Render!
    imshow( window_name, frame );

    if( waitKey(10) == 27 ) { // quit on ESC
      cout << "quitting!" << endl;
      break;
    }
  }

return 0;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



match get_region( Mat& image,
		  int canny_low, int canny_high, int canny_kernel,
		  double circularity_threshold, int min_area ) {
  //benchmark b("get_region");

  //convert to grayscale and find edges
  Mat gray;
  cvtColor( image, gray, COLOR_BGR2GRAY );
  blur(gray,gray,Size(3,3));
  Canny( gray, gray, canny_low, canny_high, canny_kernel );

  //get contours
  vector< vector<Point> > contours;
  vector<Vec4i> hierarchy;
  findContours( gray, contours, hierarchy, RETR_LIST, CHAIN_APPROX_SIMPLE );

  //get bounding boxes for acceptably compact contours
  vector<Rect> boxes;
  for ( vector<Point> c : contours ) {
    Rect box = boundingRect(c);
    if ( ( box.width*box.height ) >= min_area ) {
      double perimeter = arcLength(c,true);
      double area = contourArea(c);
      double circularity = 4*3.1415*area / ( perimeter * perimeter );
      if ( circularity > circularity_threshold &&
		(box.width*box.height) >= min_area ) {
	// box is acceptably square and not too small
	boxes.push_back(box);
      }
    }
  }

  if ( boxes.empty() ) {
    //no good boxes
    match m;
    m.good = false;
    return m;
  }
  
  //get biggest acceptable box
  Rect biggest( 0, 0, 1, 1 );
  for ( Rect box: boxes ) {
    if ( (box.width * box.height) > (biggest.width * biggest.height) ) {
      biggest = box;
    }
  }

  match m;
  m.good = true;
  m.region = biggest;
  return m;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



type_match get_match( Mat region_, vector<tmplt>& templates,
		  int canny_match_low, int canny_match_high, int canny_match_kernel,
		  int vertical_segments, int horizontal_segments) {
  Mat region = region_.clone();
  
  cvtColor( region, region, COLOR_BGR2GRAY ); // convert to grayscale
  blur( region, region, Size(3,3) ); // blur for less noise
  Canny( region, region, canny_match_low, canny_match_high, canny_match_kernel );  // detect edges

  //build tmplt from region
  tmplt roi;
  roi.region_avg = 0;
  int dx = region.cols / horizontal_segments;
  int dy = region.rows / vertical_segments;
  for (int y=0; y<=region.rows-dy; y+=dy) {
    for (int x=0; x<=region.cols-dx; x+=dx) {
      Mat segment ( region, Rect ( x, y, dx, dy ) );
      int count = countNonZero( segment ); // count number of nonzero pixels
      double fraction = (double) count / ( dx*dy ); // get fraction of nonzero pixels
      roi.regions.push_back( fraction ); // store fraction
      roi.region_avg += fraction;
    }
  }
  roi.region_avg /= roi.regions.size();
  
  //get best match
  type_match best;
  tmplt dummy;
  best.name = "unknown";
  best.match_level = 0;
  for ( tmplt templ : templates ) {
    type_match m;
    m.name = templ.name;
    m.signal = templ.signal;
    //compute correlation
    double num = 0;
    double denom1 = 0;
    double denom2 = 0;
    for (int i=0; i<roi.regions.size(); i++) {
      double a = roi.regions[i]   - roi.region_avg;
      double b = templ.regions[i] - templ.region_avg;

      num += a*b;
      denom1 += a*a;
      denom2 += b*b;
    }
    m.match_level = num / sqrt(denom1 * denom2);
    if ( m.match_level > best.match_level ) {
      best = m;
    }
  }
  return best;
}
