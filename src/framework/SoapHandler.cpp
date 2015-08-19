/*
	Copyright 2009-2012, Sumeet Chhetri

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
/*
 * SoapHandler.cpp
 *
 *  Created on: Jun 17, 2012
 *      Author: Sumeet
 */

#include "SoapHandler.h"

void SoapHandler::handle(HttpRequest* req, HttpResponse* res, void* dlib)
{
	Logger logger = LoggerFactory::getLogger("SoapHandler");
	string wsUrl = "http://" + ConfigurationData::getInstance()->coreServerProperties.ip_address + "/";
	string acurl = req->getActUrl();
	StringUtil::replaceFirst(acurl,"//","/");
	if(acurl.length()>1)
		acurl = acurl.substr(1);
	if(acurl.find(req->getCntxt_name())!=0)
		acurl = req->getCntxt_name() + "/" + acurl;
	wsUrl += acurl;
	RegexUtil::replace(acurl,"[/]+","/");
	logger << ("WsUrl is " + wsUrl) << endl;

	string xmlcnttype = CommonUtils::getMimeType(".xml");
	string meth,ws_name,env;
	ws_name = ConfigurationData::getInstance()->wsdlMap[req->getCntxt_name()][wsUrl];
	Element* soapenv = NULL;
	logger.info("request => "+req->getContent());
	Element* soapbody = NULL;
	try
	{
		XmlParser parser("Validator");
		Document doc;
		parser.parse(req->getContent(), doc);
		soapenv = &(doc.getRootElement());
		//logger << soapenv->getTagName() << "----\n" << flush;

		/*if(soapenv->getChildElements().size()==1
				&& StringUtil::toLowerCopy(soapenv->getChildElements().at(0)->getTagName())=="body")
			soapbody = soapenv->getChildElements().at(0);
		else if(soapenv->getChildElements().size()==2
				&& StringUtil::toLowerCopy(soapenv->getChildElements().at(1)->getTagName())=="body")
			soapbody = soapenv->getChildElements().at(1);*/

		soapbody = soapenv->getElementByNameIgnoreCase("body");
		if(soapbody == NULL) {
			throw "SOAP Body not found in request";
		}

		//logger << soapbody->getTagName() << "----\n" << flush;
		Element* method = soapbody->getChildElements().at(0);
		//logger << method.getTagName() << "----\n" << flush;
		meth = method->getTagName();
		string methodname = req->getCntxt_name() + meth + ws_name;
		logger << methodname << "----\n" << flush;
		void *mkr = dlsym(dlib, methodname.c_str());
		if(mkr!=NULL)
		{
			typedef string (*WsPtr) (Element*);
			WsPtr f =  (WsPtr)mkr;
			string outpt = f(method);
			typedef map<string,string> AttributeList;
			AttributeList attl = soapbody->getAttributes();
			AttributeList::iterator it;
			string bod = "<" + soapbody->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				bod.append(" " + it->first + "=\"" + it->second + "\" ");
			}
			bod.append(">"+outpt + "</" + soapbody->getTagNameSpc()+">");
			attl = soapenv->getAttributes();
			env = "<" + soapenv->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				env.append(" " + it->first + "=\"" + it->second + "\" ");
			}
			env.append(">"+bod + "</" + soapenv->getTagNameSpc()+">");
			//delete mkr;
		}
		else
		{
			AttributeList attl = soapbody->getAttributes();
			AttributeList::iterator it;
			string bod = "<" + soapbody->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				bod.append(" " + it->first + "=\"" + it->second + "\" ");
			}
			bod.append("><soap-fault><faultcode>soap:Server</faultcode><faultstring>Operation not supported</faultstring><faultactor/><detail>No such method error</detail><soap-fault></" + soapbody->getTagNameSpc()+">");
			attl = soapenv->getAttributes();
			env = "<" + soapenv->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				env.append(" " + it->first + "=\"" + it->second + "\" ");
			}
			env.append(">"+bod + "</" + soapenv->getTagNameSpc()+">");
		}
		logger << "\n----------------------------------------------------------------------------\n" << flush;
		logger << env << "\n----------------------------------------------------------------------------\n" << flush;
	}
	catch(const char* faultc)
	{
		string fault(faultc);
		string bod = "", btag = "";
		AttributeList attl;
		AttributeList::iterator it;
		if(soapbody!=NULL)
		{
			attl = soapbody->getAttributes();
			btag = soapbody->getTagNameSpc();
			bod = "<" + soapbody->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				bod.append(" " + it->first + "=\"" + it->second + "\" ");
			}
		}
		else
		{
			btag = soapenv->getNameSpc() + ":body";
			bod = "<" + btag;
		}
		bod.append("><soap-fault><faultcode>soap:Server</faultcode><faultstring>"+fault+"</faultstring><detail></detail><soap-fault></" + btag+">");
		attl = soapenv->getAttributes();
		env = "<" + soapenv->getTagNameSpc();
		for(it=attl.begin();it!=attl.end();it++)
		{
			env.append(" " + it->first + "=\"" + it->second + "\" ");
		}
		env.append(">"+bod + "</" + soapenv->getTagNameSpc()+">");
		logger << ("Soap fault - " + fault) << flush;
	}
	catch(const string &fault)
	{
		string bod = "", btag = "";
		AttributeList attl;
		AttributeList::iterator it;
		if(soapbody!=NULL)
		{
			attl = soapbody->getAttributes();
			btag = soapbody->getTagNameSpc();
			bod = "<" + soapbody->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				bod.append(" " + it->first + "=\"" + it->second + "\" ");
			}
		}
		else
		{
			btag = soapenv->getNameSpc() + ":body";
			bod = "<" + btag;
		}
		bod.append("><soap-fault><faultcode>soap:Server</faultcode><faultstring>"+fault+"</faultstring><detail></detail><soap-fault></" + btag+">");
		attl = soapenv->getAttributes();
		env = "<" + soapenv->getTagNameSpc();
		for(it=attl.begin();it!=attl.end();it++)
		{
			env.append(" " + it->first + "=\"" + it->second + "\" ");
		}
		env.append(">"+bod + "</" + soapenv->getTagNameSpc()+">");
		logger << ("Soap fault - " + fault) << flush;
	}
	catch(const Exception& e)
	{
		string bod = "", btag = "";
		AttributeList attl;
		AttributeList::iterator it;
		if(soapbody!=NULL)
		{
			attl = soapbody->getAttributes();
			btag = soapbody->getTagNameSpc();
			bod = "<" + soapbody->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				bod.append(" " + it->first + "=\"" + it->second + "\" ");
			}
		}
		else
		{
			btag = soapenv->getNameSpc() + ":body";
			bod = "<" + btag;
		}
		bod.append("><soap-fault><faultcode>soap:Server</faultcode><faultstring>"+e.getMessage()+"</faultstring><detail></detail><soap-fault></" + btag+">");
		attl = soapenv->getAttributes();
		env = "<" + soapenv->getTagNameSpc();
		for(it=attl.begin();it!=attl.end();it++)
		{
			env.append(" " + it->first + "=\"" + it->second + "\" ");
		}
		env.append(">"+bod + "</" + soapenv->getTagNameSpc()+">");
		logger << ("Soap fault - " + e.getMessage()) << flush;
	}
	catch(...)
	{
		string bod = "", btag = "";
		AttributeList attl;
		AttributeList::iterator it;
		if(soapbody!=NULL)
		{
			attl = soapbody->getAttributes();
			btag = soapbody->getTagNameSpc();
			bod = "<" + soapbody->getTagNameSpc();
			for(it=attl.begin();it!=attl.end();it++)
			{
				bod.append(" " + it->first + "=\"" + it->second + "\" ");
			}
		}
		else
		{
			btag = soapenv->getNameSpc() + ":body";
			bod = "<" + btag;
		}
		bod.append("><soap-fault><faultcode>soap:Server</faultcode><faultstring>Standard Exception</faultstring><detail></detail><soap-fault></" + btag+">");
		attl = soapenv->getAttributes();
		env = "<" + soapenv->getTagNameSpc();
		for(it=attl.begin();it!=attl.end();it++)
		{
			env.append(" " + it->first + "=\"" + it->second + "\" ");
		}
		env.append(">"+bod + "</" + soapenv->getTagNameSpc()+">");
		logger << "Soap Standard Exception" << flush;
	}
	res->setHTTPResponseStatus(HTTPResponseStatus::Ok);
	res->addHeaderValue(HttpResponse::ContentType, xmlcnttype);
	res->setContent(env);
	res->setDone(true);
}
