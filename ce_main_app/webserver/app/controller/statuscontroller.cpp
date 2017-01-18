#include "statuscontroller.h"
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include "statusreport.h"
#include "statuscontroller.h"

StatusController::StatusController(ConfigService* pxDateService, FloppyService* pxFloppyService):pxDateService(pxDateService),pxFloppyService(pxFloppyService)
{
}

StatusController::~StatusController()
{
}

bool StatusController::indexAction(mg_connection *conn, mg_request_info *req_info)
{
	std::ifstream fileTemplateLayout("/ce/app/webroot/templates/layout.tpl");
	std::string sTemplateLayout((std::istreambuf_iterator<char>(fileTemplateLayout)),std::istreambuf_iterator<char>());
	std::ifstream fileTemplateDebug("/ce/app/webroot/templates/status.tpl");
	std::string sTemplateDebug((std::istreambuf_iterator<char>(fileTemplateDebug)),std::istreambuf_iterator<char>());

	std::map<std::string, std::string> mapVariables;
	//get status report
	std::string sStatusReport;
	StatusReport xStatusReport;
	xStatusReport.createReport(sStatusReport,REPORTFORMAT_HTML_ONLYBODY);  
	mapVariables["statusreport"]=sStatusReport;

    std::string sOutput=replaceAll(sTemplateLayout,std::string("{{title}}"),std::string("CosmosEx status"));
	sOutput=replaceAll(sOutput,std::string("{{content}}"),sTemplateDebug);
	sOutput=replaceAll(sOutput,std::string("{{activeStatus}}"),"active");
	sOutput=replaceAll(sOutput,std::string("{{info}}"),"This is your CosmosEx current status.");

	std::map<std::string, std::string>::iterator pxVarIter;
    for (pxVarIter = mapVariables.begin(); pxVarIter != mapVariables.end(); ++pxVarIter) {
    	std::string sTemplatePlaceHolder="{{"+pxVarIter->first+"}}";
	    sOutput=replaceAll(sOutput,sTemplatePlaceHolder,pxVarIter->second);
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long)sOutput.length());        // Always set Content-Length
    mg_write(conn, sOutput.c_str(), sOutput.length());
    return true;
}
