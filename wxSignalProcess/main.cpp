#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/tokenzr.h>
#include <wx/valnum.h>
#include <wx/thread.h>
#include "mathplot.h"
#include "connectargsdlg.h"
#include "serialport.h"

#include <fstream>
#include <cmath>
#include <cstdlib>
#include <ctime> 
#include <iostream>
#include <chrono> 

enum{
   ID_EXIT = 200,
   ID_OPEN,
   ID_RECORD,
   ID_VCP,
   ID_CLEAR_ALL_PLOT,
   ID_CONNECT_DEVICE
};

enum{
   EVT_SERIAL_PORT_READ = 625,
   EVT_REFRESH_PLOT
};

class Thread;

class App:public wxApp
{
   public:
      bool OnInit();
};

class Frame:public wxFrame
{
   public:
      Frame(const wxString&);
      ~Frame();

      void CreateUI();

      void OnProcess(wxCommandEvent&);

      void OnRecord(wxCommandEvent&);
      void OnClearAllPlot(wxCommandEvent&);
      void OnOpen(wxCommandEvent&);
      void OnKeyDown(wxKeyEvent& event);
      void OnConnectDevice(wxCommandEvent&);
      void OnExit(wxCommandEvent&);

      void ListDevices(wxMenuEvent&);

      void ClearAllData();

      void OnFit(wxCommandEvent&);

      void OnThreadEvent(wxThreadEvent &event);
      
   private:

      wxMenu *vcp;
      wxThread *thread;

      SerialPort serial;     
      bool is_open;

      bool run_flag;
      
      mpWindow *original_plot;
      mpWindow *processing_plot;  

      mpFXYVector* original_layer;
      std::vector<double> original_layer_x, original_layer_y;
      mpFXYVector* processing_layer;
      std::vector<double> processing_layer_x, processing_layer_y;      

      std::chrono::steady_clock::time_point record_start_time;
      bool record_flag;
      std::fstream record;

      DECLARE_EVENT_TABLE();
};

class Thread:public wxThread
{
   public:
      Thread(Frame*,wxEvtHandler*);

      void* Entry();
   
   private:
      Frame *frame;
      wxEvtHandler *handler;
};

IMPLEMENT_APP(App)
DECLARE_APP(App)

BEGIN_EVENT_TABLE(Frame,wxFrame)
   EVT_MENU(ID_EXIT,Frame::OnExit)
   EVT_MENU(ID_OPEN,Frame::OnOpen)
   EVT_MENU(ID_CLEAR_ALL_PLOT,Frame::OnClearAllPlot)
   EVT_MENU(ID_RECORD,Frame::OnRecord)
   EVT_MENU(ID_CONNECT_DEVICE,Frame::OnConnectDevice)
   EVT_MENU(mpID_FIT, Frame::OnFit)
   EVT_THREAD(wxID_ANY, Frame::OnThreadEvent)
END_EVENT_TABLE()

bool App::OnInit()
{
   Frame *frame = new Frame(wxT("wxSignalProcess"));

   frame->Show(true);

   return true;
}

Frame::Frame(const wxString &title):wxFrame(NULL,wxID_ANY,title,wxDefaultPosition,wxSize(1050,700),wxMINIMIZE_BOX | wxCLOSE_BOX | wxCAPTION | wxSYSTEM_MENU)
{
   run_flag = true;
   record_flag = false;

   CreateUI();

   Bind(wxEVT_CHAR_HOOK,&Frame::OnKeyDown,this);

   original_layer = new mpFXYVector(wxT("Original Signal"),mpALIGN_NE);
   original_layer->ShowName(false);
   original_layer->SetContinuity(true);
   wxPen vectorpen(*wxRED, 2, wxSOLID);
   original_layer->SetPen(vectorpen);
   original_layer->SetDrawOutsideMargins(false);

   wxFont graphFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   mpScaleX* xaxis = new mpScaleX(wxT("Time（index）"), mpALIGN_BOTTOM, true, mpX_NORMAL);
   mpScaleY* yaxis = new mpScaleY(wxT("Amplitude（mV）"), mpALIGN_LEFT, true);
   xaxis->SetFont(graphFont);
   yaxis->SetFont(graphFont);
   xaxis->SetDrawOutsideMargins(false);
   yaxis->SetDrawOutsideMargins(false);
   original_plot->SetMargins(30, 30, 50, 100);
   original_plot->AddLayer(     xaxis );
   original_plot->AddLayer(     yaxis );
   original_plot->AddLayer(     original_layer );
   wxBrush hatch(wxColour(200,200,200), wxSOLID);

   mpInfoLegend* leg;
   original_plot->AddLayer( leg = new mpInfoLegend(wxRect(200,20,40,40), wxTRANSPARENT_BRUSH));
   leg->SetVisible(true);

   original_plot->EnableDoubleBuffer(true);
   original_plot->SetMPScrollbars(false);
   original_plot->Fit();

   processing_layer = new mpFXYVector(wxT("Processing Signal"),mpALIGN_NE);
   processing_layer->ShowName(false);
   processing_layer->SetContinuity(true);
   wxPen vectorpen1(*wxBLACK, 2, wxSOLID);
   processing_layer->SetPen(vectorpen1);
   processing_layer->SetDrawOutsideMargins(false);

   xaxis = new mpScaleX(wxT("Time（index）"), mpALIGN_BOTTOM, true, mpX_NORMAL);
   yaxis = new mpScaleY(wxT("Amplitude（mV）"), mpALIGN_LEFT, true);
   xaxis->SetFont(graphFont);
   yaxis->SetFont(graphFont);
   xaxis->SetDrawOutsideMargins(false);
   yaxis->SetDrawOutsideMargins(false);
   processing_plot->SetMargins(30, 30, 50, 100);
   processing_plot->AddLayer(     xaxis );
   processing_plot->AddLayer(     yaxis );
   processing_plot->AddLayer(     processing_layer );

   processing_plot->AddLayer( leg = new mpInfoLegend(wxRect(200,20,40,40), wxTRANSPARENT_BRUSH));
   leg->SetVisible(true);

   processing_plot->EnableDoubleBuffer(true);
   processing_plot->SetMPScrollbars(false);
   processing_plot->Fit();
   
/*
   processing_plot->SetMargins(30, 30, 50, 100);
   processing_plot->AddLayer(     xaxis );
   processing_plot->AddLayer(     yaxis );
   processing_plot->AddLayer(     original_layer );
   processing_plot->AddLayer( leg = new mpInfoLegend(wxRect(200,20,40,40), wxTRANSPARENT_BRUSH));
   processing_plot->EnableDoubleBuffer(true);
   processing_plot->SetMPScrollbars(false);
   processing_plot->Fit(); 
  */
   std::srand(std::time(nullptr));
   
   
   thread = new Thread(this,this);
   thread->Create();
   thread->Run();
}

Frame::~Frame()
{
   if(record.is_open()){
      record.close();
   }
   if(thread){
      thread->Delete();
      thread = NULL;
   }
}

void Frame::CreateUI()
{
   wxMenu *file = new wxMenu;
   file->Append(ID_OPEN,wxT("O&pen\tAlt-o"),wxT("Open"));
   file->Append(ID_RECORD,wxT("R&cord\tAlt-r"),wxT("Record"));
   file->Append(ID_EXIT,wxT("E&xit\tAlt-e"),wxT("Exit"));

   vcp = new wxMenu();
   wxMenu *tools = new wxMenu;
   //tools->AppendSubMenu(vcp,wxT("V&CP\tAlt-v"),wxT("Virtual COM Port"));
   tools->Append(ID_CONNECT_DEVICE,wxT("V&CP\tAlt-v"),wxT("Virtual COM Port"));
   //vcp->Bind(wxEVT_MENU_HIGHLIGHT,&Frame::ListDevices,this);   
   Bind(wxEVT_MENU_HIGHLIGHT, wxMenuEventHandler(Frame::ListDevices), vcp);

   wxMenu *view = new wxMenu;
   view->Append(mpID_FIT,wxT("F&it\tAlt-f"),wxT("Fit"));
   view->Append(ID_CLEAR_ALL_PLOT,wxT("C&lear Plot\tAlt-c"),wxT("Clear All Plot"));
  
   wxMenuBar *bar = new wxMenuBar;

   bar->Append(file,wxT("File"));
   bar->Append(tools,wxT("Tools"));
   bar->Append(view,wxT("View"));
   SetMenuBar(bar);

   original_plot = new mpWindow( this, -1, wxPoint(15,10), wxSize(1020,300), wxBORDER_SUNKEN );
   processing_plot = new mpWindow( this, -1, wxPoint(15,330), wxSize(1024,300), wxBORDER_SUNKEN );

   CreateStatusBar(1);
   SetStatusText(wxT("wxSignalProcess"));   
}

void Frame::OnFit( wxCommandEvent &WXUNUSED(event) )
{
   original_plot->Fit();
   processing_plot->Fit();
}

void Frame::OnClearAllPlot(wxCommandEvent &event)
{
   ClearAllData();
}

void Frame::ClearAllData()
{
   original_plot->DelAllLayers(true);
   processing_plot->DelAllLayers(true);

   original_layer_x.clear();
   original_layer_y.clear();

}

void Frame::OnOpen(wxCommandEvent &event)
{
   wxFileDialog 
        ofd(this, wxT("Open RAW Data file"), "", "",
                       "RAW Data files (*.*)|*.*", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if(ofd.ShowModal() == wxID_CANCEL){
        return;     // the user changed idea...
    }
    //LoadFile(ofd.GetPath());
}

void Frame::OnRecord(wxCommandEvent &event)
{
   /*
   if(!is_load_file){
      wxMessageBox(wxT("Not Load File"));
      return ;
   }*/

   wxFileDialog fileDialog(this, _("CSV"), wxT(""), wxT("data"), wxT("CSV (*.csv)|.csv"), wxFD_SAVE);
   if(fileDialog.ShowModal() == wxID_OK) {
      wxFileName namePath(fileDialog.GetPath());
      record.open(fileDialog.GetPath().mb_str(),std::fstream::out);
      record_flag = true;
      record_start_time = std::chrono::steady_clock::now();
      /*
      int fileType = wxBITMAP_TYPE_BMP;
      if( namePath.GetExt().CmpNoCase(wxT("jpeg")) == 0 ) fileType = wxBITMAP_TYPE_JPEG;
      if( namePath.GetExt().CmpNoCase(wxT("jpg")) == 0 )  fileType = wxBITMAP_TYPE_JPEG;
      if( namePath.GetExt().CmpNoCase(wxT("png")) == 0 )  fileType = wxBITMAP_TYPE_PNG;
      wxSize imgSize(800,600);
      original_plot->SaveScreenshot(fileDialog.GetPath(), fileType, imgSize, false);
      */
   }
   else{
      return;
   }

   event.Skip();
}

void Frame::ListDevices(wxMenuEvent &event)
{
   //vcp->Append(wxID_ANY,wxT("i1"));
   //wxMenuItem* menuItemSettings = new wxMenuItem(this,wxID_ANY,wxT("Settings"),wxEmptyString,wxITEM_NORMAL);
   //vcp->Append(menuItemSettings);
   //vcp->AppendSeparator();
   
   //std::cout << "try" << std::endl;
}

void Frame::OnConnectDevice(wxCommandEvent &event)
{
   ConnectArgsDialog dlg(this,wxID_ANY,wxT("Connect"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);

   if(dlg.ShowModal() == wxID_OK){
      is_open = serial.Open(dlg.GetDevicePath().mb_str());
      serial.SetBaudRate(wxAtoi(dlg.GetBaudRate()));
      serial.SetParity(8,1,'N');

      run_flag = true;

      if(is_open){
	 unsigned char gid[4] = "ACK";
	 serial.Write(gid,4);

	 unsigned char sms[4] = "GOT";
	 serial.Write(sms,4);  
      }
      else{
	 wxMessageBox(wxT("Serial Port Error!"),wxT("Error!"));
      }

   }
}

void Frame::OnKeyDown(wxKeyEvent& event)
{
   //wxMessageBox(wxString::Format("KeyDown: %i\n", (int)event.GetKeyCode()));
   if(event.GetKeyCode() == 32){
      run_flag ^= true;
   }
   event.Skip();
}

void Frame::OnExit(wxCommandEvent &event)
{
   Close();
}

unsigned int sampling_rate = 0;
std::chrono::steady_clock::time_point sampling_time = std::chrono::steady_clock::now();
int index_point = 0;
void Frame::OnThreadEvent(wxThreadEvent &event)
{
   const size_t MAX_POINT = 10000;

   if(event.GetInt() == EVT_SERIAL_PORT_READ && is_open && run_flag){

      unsigned char buffer[3000]={0};
      int length = serial.Read(buffer);

      if((std::chrono::steady_clock::duration(std::chrono::steady_clock::now() - sampling_time).count() > std::chrono::steady_clock::period::den) && run_flag){    
	 wxLogDebug(wxT("Sampling Rate: %d"),sampling_rate);
	 sampling_rate = 0;
	 sampling_time = std::chrono::steady_clock::now();
      }

      if(length != -1){
	 //std::cout << buffer << std::endl;
	 
	 wxStringTokenizer tokenizer1(buffer,"R");
	 if(tokenizer1.CountTokens() == 0){
	    return ;
	 }
	 tokenizer1.GetNextToken();

	 while(tokenizer1.HasMoreTokens()){
	    wxString split = tokenizer1.GetNextToken();
	    ++sampling_rate;
	    
	    if(record_flag){
	       record << split << std::endl;
	       if(std::chrono::steady_clock::duration(std::chrono::steady_clock::now() - record_start_time).count() > 60 * std::chrono::steady_clock::period::den){
		  record_flag = false;
		  record.close();
		  wxMessageBox(wxT("Record 1 minumte data!"),wxT("DONE!"));
	       }
	    }

	    wxStringTokenizer tokenizer2(split,",");
	    if(tokenizer2.CountTokens() < 6){
	       return ;
	    }
	    int token_index = 0;
	    while(tokenizer2.HasMoreTokens()){
	       wxString str = tokenizer2.GetNextToken();       
	       if(token_index == 1){
		  original_layer_x.push_back( index_point );
		  original_layer_y.push_back(wxAtoi(str));
		  if (original_layer_x.size() > MAX_POINT && original_layer_y.size() > MAX_POINT){
		     original_layer_x.erase(original_layer_x.begin());
		     original_layer_y.erase(original_layer_y.begin());
		  } 
	       }
	       if(token_index == 2){
		  processing_layer_x.push_back( index_point );
		  processing_layer_y.push_back(wxAtoi(str));
		  if (processing_layer_x.size() > MAX_POINT && processing_layer_y.size() > MAX_POINT){
		     processing_layer_x.erase(processing_layer_x.begin());
		     processing_layer_y.erase(processing_layer_y.begin());
		  } 
		  ++index_point;
	       }		       
	       ++token_index;
	    }
	 }

      }  
   }

   if(event.GetInt() == EVT_REFRESH_PLOT && is_open && run_flag){
      original_layer->SetData(original_layer_x, original_layer_y);
      original_plot->Fit();
      processing_layer->SetData(processing_layer_x, processing_layer_y);
      processing_plot->Fit();      
   }
   //std::cout << ccc << std::endl;
}

Thread::Thread(Frame *parent,wxEvtHandler *evt):wxThread(wxTHREAD_DETACHED),handler(evt)
{
   frame = parent;
}

std::chrono::steady_clock::time_point refresh_last = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point read_last = std::chrono::steady_clock::now();
void* Thread::Entry()
{
   const int32_t READ_RATE = 1000;
   const int32_t FRAME_RATE = 30;

   while(!TestDestroy()){
      if(std::chrono::steady_clock::duration(std::chrono::steady_clock::now() - read_last).count() > (std::chrono::steady_clock::period::den/READ_RATE)){
	 wxThreadEvent *evt = new wxThreadEvent(wxEVT_THREAD);
	 evt->SetInt(EVT_SERIAL_PORT_READ);
	 handler->QueueEvent(evt);
	 read_last = std::chrono::steady_clock::now();
      }
      if(std::chrono::steady_clock::duration(std::chrono::steady_clock::now() - refresh_last).count() > (std::chrono::steady_clock::period::den/FRAME_RATE)){
	 wxThreadEvent *evt = new wxThreadEvent(wxEVT_THREAD);
	 evt->SetInt(EVT_REFRESH_PLOT);
	 handler->QueueEvent(evt);
	 refresh_last = std::chrono::steady_clock::now();
      }
   }

   return NULL;
}
