    #include "debugcontroller.h"
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include "version.h"
#include "statusreport.h"
#include "../../../config/configstream.h"

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
	//get status report
	std::string sStatusReport;
	StatusReport xStatusReport;
	xStatusReport.createReport(sStatusReport,REPORTFORMAT_HTML_ONLYBODY);  
	mapVariables["statusreport"]=sStatusReport;

    std::string sOutput=replaceAll(sTemplateLayout,std::string("{{title}}"),std::string("CosmosEx debug information"));
	sOutput=replaceAll(sOutput,std::string("{{content}}"),sTemplateDebug);
	sOutput=replaceAll(sOutput,std::string("{{activeHome}}"),"active");
	sOutput=replaceAll(sOutput,std::string("{{info}}"),"Some internal information. Just in case.");

	std::map<std::string, std::string>::iterator pxVarIter;
    for (pxVarIter = mapVariables.begin(); pxVarIter != mapVariables.end(); ++pxVarIter) {
    	std::string sTemplatePlaceHolder="{{"+pxVarIter->first+"}}";
	    sOutput=replaceAll(sOutput,sTemplatePlaceHolder,pxVarIter->second);
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %d\r\n\r\n",sOutput.length());        // Always set Content-Length
    mg_write(conn, sOutput.c_str(), sOutput.length());
    return true;
}

bool DebugController::getlogAction(mg_connection *conn, mg_request_info *req_info)
{
    std::string sDownloadedFileName = "ce_log.txt";
    std::string sFileType           = "text/plain";
    std::string sCeFilePath         = "/var/log/ce.log";
    
    // don't send content length, filesize changes as we read
    return getFile(conn, sDownloadedFileName, sFileType, sCeFilePath, false);
}

bool DebugController::getConfigAction(mg_connection *conn, mg_request_info *req_info)
{
    std::string sDownloadedFileName = "ce_config.txt";
    std::string sFileType           = "text/plain";
    std::string sCeFilePath         = CONFIG_TEXT_FILE;
    
    return getFile(conn, sDownloadedFileName, sFileType, sCeFilePath, true);
}

bool DebugController::action_get_ceconf_prg(mg_connection *conn, mg_request_info *req_info)
{
    std::string sDownloadedFileName = "ce_conf.prg";
    std::string sFileType           = "application/octet-stream";
    std::string sCeFilePath         = "/ce/app/configdrive/ce_conf.prg";
    
    return getFile(conn, sDownloadedFileName, sFileType, sCeFilePath, true);
}

bool DebugController::action_get_ceconf_msa(mg_connection *conn, mg_request_info *req_info)
{
    std::string sDownloadedFileName = "ce_conf.msa";
    std::string sFileType           = "application/octet-stream";
    std::string sCeFilePath         = "/ce/app/ce_conf.msa";
    
    return getFile(conn, sDownloadedFileName, sFileType, sCeFilePath, true);
}

bool DebugController::action_get_ceconf_tar(mg_connection *conn, mg_request_info *req_info)
{
    std::string sDownloadedFileName = "ce_conf.tar";
    std::string sFileType           = "application/octet-stream";
    std::string sCeFilePath         = "/ce/app/ce_conf.tar";
    
    return getFile(conn, sDownloadedFileName, sFileType, sCeFilePath, true);
}

bool DebugController::action_get_cedd(mg_connection *conn, mg_request_info *req_info)
{
    std::string sDownloadedFileName = "ce_dd.prg";
    std::string sFileType           = "application/octet-stream";
    std::string sCeFilePath         = "/ce/app/configdrive/ce_dd.prg";
    
    return getFile(conn, sDownloadedFileName, sFileType, sCeFilePath, true);
}

bool DebugController::getFile(mg_connection *conn, std::string &sDownloadedFileName, std::string &sContentType, std::string &sCeFilePath, bool sendFileSize)
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    
    std::string sContent="Content-Type: " + sContentType + "\r\n";
	mg_write(conn, sContent.c_str(), sContent.length());
    
    mg_printf(conn, "Cache: no-cache\r\n");
    
    std::string sHeader="Content-Disposition: attachment; filename=\"" + sDownloadedFileName + "\"\r\n";
    mg_write(conn, sHeader.c_str(), sHeader.length());

	std::ifstream file;
    file.open(sCeFilePath.c_str(), std::ios::in|std::ios::binary); //open a file in read only mode

	if(sendFileSize) {
        file.seekg( 0, std::ios::end );
        int iFileSize=file.tellg();
        file.seekg( 0, std::ios::beg );
        mg_printf(conn, "Content-Length: %d\r\n",iFileSize);
	}

    mg_printf(conn, "\r\n");

    char* pcBuffer = new char[1024];

	//send 1024 byte chunks (note: file is being appended by other thread while we read)
    while( file.read( pcBuffer, 1024 ) )
	{
        mg_write(conn,pcBuffer,file.gcount());
	}
    if( file.gcount()>0 )
    {
        mg_write(conn,pcBuffer,file.gcount());
    }
    file.close();

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
