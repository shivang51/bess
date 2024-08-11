#include "ui/dialogs.h"

#include <Windows.h>
#include <filesystem>
#include <iostream>

namespace Bess::UI {
    std::string Dialogs::showOpenFileDialog(const std::string& title, const std::string& filters)
    {
        OPENFILENAMEA ofn = { 0 };
        std::string filename;
        filename.resize(250);
        std::string filter;

        auto current_dir = std::filesystem::current_path();

        if (filters == "")
        {
            filter = "All\0*.*\0";
        }
        else
        {
            for (auto ch : filters)
            {
                if (ch == '0')
                    ch = '\0';
                filter += ch;
            }
        }


        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = (LPSTR)filename.c_str();
        ofn.nMaxFile = 250;
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrTitle = (LPCSTR)title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_DONTADDTORECENT;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            SetCurrentDirectoryA((LPCSTR)current_dir.string().c_str());
            return filename;
        }
        else
        {
            switch (CommDlgExtendedError())
            {
            case CDERR_DIALOGFAILURE: std::cout << "CDERR_DIALOGFAILURE\n";   break;
            case CDERR_FINDRESFAILURE: std::cout << "CDERR_FINDRESFAILURE\n";  break;
            case CDERR_INITIALIZATION: std::cout << "CDERR_INITIALIZATION\n";  break;
            case CDERR_LOADRESFAILURE: std::cout << "CDERR_LOADRESFAILURE\n";  break;
            case CDERR_LOADSTRFAILURE: std::cout << "CDERR_LOADSTRFAILURE\n";  break;
            case CDERR_LOCKRESFAILURE: std::cout << "CDERR_LOCKRESFAILURE\n";  break;
            case CDERR_MEMALLOCFAILURE: std::cout << "CDERR_MEMALLOCFAILURE\n"; break;
            case CDERR_MEMLOCKFAILURE: std::cout << "CDERR_MEMLOCKFAILURE\n";  break;
            case CDERR_NOHINSTANCE: std::cout << "CDERR_NOHINSTANCE\n";     break;
            case CDERR_NOHOOK: std::cout << "CDERR_NOHOOK\n";          break;
            case CDERR_NOTEMPLATE: std::cout << "CDERR_NOTEMPLATE\n";      break;
            case CDERR_STRUCTSIZE: std::cout << "CDERR_STRUCTSIZE\n";      break;
            case FNERR_BUFFERTOOSMALL: std::cout << "FNERR_BUFFERTOOSMALL\n";  break;
            case FNERR_INVALIDFILENAME: std::cout << "FNERR_INVALIDFILENAME\n"; break;
            case FNERR_SUBCLASSFAILURE: std::cout << "FNERR_SUBCLASSFAILURE\n"; break;
            default: std::cout << "You cancelled.\n";
            }
        }
        return "";
    }
}