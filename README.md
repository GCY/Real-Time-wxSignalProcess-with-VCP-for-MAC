# Real-Time-wxSignalProcess-with-VCP-for-MAC

Dependency：
</br>
wxWidgets 3.0 : https://github.com/wxWidgets
</br>
wxMathPlot 0.1.2 (MRPT version) : https://github.com/MRPT/mrpt
</br>
POSIX API
</br>
C++11
</br></br>
build wxSignalProcess:
</br>
g++ -o2 -o main.app main.cpp mathplot.cpp connectargsdlg.cpp serialport.cpp \`wx-config --cxxflags --libs\` --std=c++11
</br></br>
build STM32F407 with ADC+VCP:
</br>
makefile
</br></br>
demo video:
</br>
[![Audi R8](http://img.youtube.com/vi/3fFxT0YeQAM/0.jpg)](https://youtu.be/3fFxT0YeQAM)
