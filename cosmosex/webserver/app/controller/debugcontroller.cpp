#include "debugcontroller.h"
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include "version.h"

DebugController::DebugController(ConfigService* pxDateService, FloppyService* pxFloppyService):pxDateService(pxDateService),pxFloppyService(pxFloppyService)
{
}

DebugController::~DebugController()
{
}

bool DebugController::indexAction(mg_connection *conn, mg_request_info *req_info)
{
	std::ifstream fileTemplateLayout("/ce/app/webroot/templates/layout.tpl");
	std::string sTemplateLayout((std::istreambuf_iterator<char>(fileTemplateLayout)),std::istreambuf_iterator<char>());
	std::ifstream fileTemplateDebug("/ce/app/webroot/templates/debug.tpl");
	std::string sTemplateDebug((std::istreambuf_iterator<char>(fileTemplateDebug)),std::istreambuf_iterator<char>());

 	std::ostringstream stringStream;
	stringStream << "<ul>";
	for( int i = 0; i < req_info->num_headers; i++ )
	{
		stringStream << "<li>[" << req_info->http_headers[i].name << "]: [" << req_info->http_headers[i].value << "]</li>";
	}
	stringStream << "</ul>";

	std::map<std::string, std::string> mapVariables;

    char appVersion[16];
    Version::getAppVersion(appVersion);

	mapVariables["browser_headers"]=stringStream.str();
	mapVariables["version_app"]=std::string(appVersion);
	mapVariables["date"]=pxDateService->getTimeString();

    std::string sOutput=replaceAll(sTemplateLayout,std::string("{{title}}"),std::string("CosmosEx debug information"));
	sOutput=replaceAll(sOutput,std::string("{{content}}"),sTemplateDebug);

	std::map<std::string, std::string>::iterator pxVarIter;
    for (pxVarIter = mapVariables.begin(); pxVarIter != mapVariables.end(); ++pxVarIter) {
    	std::string sTemplatePlaceHolder="{{"+pxVarIter->first+"}}";
	    sOutput=replaceAll(sOutput,sTemplatePlaceHolder,pxVarIter->second);
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %d\r\n\r\n",sOutput.length());        // Always set Content-Length
    mg_printf(conn, sOutput.c_str());
    return true;

    mg_printf(conn, "<html><body>");
    mg_printf(conn, "<h2>This is the debug/index action</h2>");
    mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>",
              req_info->request_method, req_info->uri, req_info->http_version);
    mg_printf(conn, "</body></html>\n");
    return true;
}

bool DebugController::getlogAction(mg_connection *conn, mg_request_info *req_info)
{
	std::string sFileName="ce_log.txt";

    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
	mg_printf(conn, "Content-Type: text/plain\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    std::string sHeader="Content-Disposition: attachment; filename=\""+sFileName+"\"\r\n";
    mg_printf(conn, sHeader.c_str());

	std::ifstream fileCeLog;
    fileCeLog.open("/var/log/ce.log", std::ios::in|std::ios::binary); //open a file in read only mode

	/*
	don't send content length, filesize changes as we read
    fileCeLog.seekg( 0, std::ios::end );
    int iFileSize=fileCeLog.tellg();
    fileCeLog.seekg( 0, std::ios::beg );
    mg_printf(conn, "Content-Length: %d\r\n",iFileSize);
	*/

    mg_printf(conn, "\r\n");

    char* pcBuffer = new char[1024];

	//send 1024 byte chunks (note: file is being appended by other thread while we read)
    while( fileCeLog.read( pcBuffer, 1024 ) )
	{
        mg_write(conn,pcBuffer,fileCeLog.gcount());
	}
    if( fileCeLog.gcount()>0 )
    {
        mg_write(conn,pcBuffer,fileCeLog.gcount());
    }
    fileCeLog.close();

    return true;

}

//based on javascript encodeURIComponent()
std::string DebugController::urlencode(const std::string &c)
{
    std::string escaped="";
    int max = c.length();
    for(int i=0; i<max; i++)
    {
        if ( (48 <= c[i] && c[i] <= 57) ||//0-9
             (65 <= c[i] && c[i] <= 90) ||//abc...xyz
             (97 <= c[i] && c[i] <= 122) || //ABC...XYZ
             (c[i]=='~' || c[i]=='!' || c[i]=='*' || c[i]=='(' || c[i]==')' || c[i]=='\'')
        )
        {
            escaped.append( &c[i], 1);
        }
        else
        {
            escaped.append("%");
            escaped.append( char2hex(c[i]) );//converts char 255 to string "ff"
        }
    }
    return escaped;
}

std::string DebugController::char2hex( char dec )
{
    char dig1 = (dec&0xF0)>>4;
    char dig2 = (dec&0x0F);
    if ( 0<= dig1 && dig1<= 9) dig1+=48;    //0,48inascii
    if (10<= dig1 && dig1<=15) dig1+=97-10; //a,97inascii
    if ( 0<= dig2 && dig2<= 9) dig2+=48;
    if (10<= dig2 && dig2<=15) dig2+=97-10;

    std::string r;
    r.append( &dig1, 1);
    r.append( &dig2, 1);
    return r;
}
