/*
 * DataSourceInterface.cpp
 *
 *  Created on: 12-Oct-2014
 *      Author: sumeetc
 */

#include "DataSourceInterface.h"

string DataSourceInterface::BLANK = "";

DataSourceInterface::DataSourceInterface() {
	dlib = NULL;
	reflector = NULL;
	mapping = NULL;
	pool = NULL;
	context = NULL;
}

DataSourceInterface::~DataSourceInterface() {
	endSession();
}

bool DataSourceInterface::executeInsertInternal(Query& query, void* entity) {
	DataSourceEntityMapping dsemp = mapping->getDataSourceEntityMapping(query.getClassName());
	ClassInfo clas = reflector->getClassInfo(query.getClassName(), appName);

	if(dsemp.isIdGenerate() && dsemp.getIdgendbEntityType()!="identity") {
		assignId(dsemp, clas, entity);
	}

	bool flag = executeInsert(query, entity);

	if(flag && dsemp.isIdGenerate() && dsemp.getIdgendbEntityType()=="identity") {
		assignId(dsemp, clas, entity);
	}

	return flag;
}


void DataSourceInterface::assignId(DataSourceEntityMapping& dsemp, ClassInfo& clas, void* entity) {
	GenericObject idv;
	next(dsemp, idv);
	if(!idv.isNull())
	{
		Field fld = clas.getField(dsemp.getIdPropertyName());
		vector<void *> valus;
		if(GenericObject::isNumber32(idv.getTypeName()) && GenericObject::isNumber(fld.getType()))
		{
			long* id;
			idv.get(id);
			valus.push_back(id);
		}
		else if(GenericObject::isNumber64(idv.getTypeName()) && GenericObject::isNumber(fld.getType()))
		{
			long long* id;
			idv.get(id);
			valus.push_back(id);
		}
		else if(GenericObject::isFPN(idv.getTypeName()) && GenericObject::isFPN(fld.getType()))
		{
			long double* id;
			idv.get(id);
			valus.push_back(id);
		}
		else if(GenericObject::isString(idv.getTypeName()) && GenericObject::isString(fld.getType()))
		{
			string* id;
			idv.set(id);
			valus.push_back(id);
		}
		else
		{
			throw "Data-Source-Object/Entity types do not match for id property" + dsemp.getClassName() + ":" + dsemp.getIdPropertyName();
		}

		args argus;
		argus.push_back(fld.getType());
		string methname = "set"+StringUtil::capitalizedCopy(fld.getFieldName());
		Method meth = clas.getMethod(methname,argus);
		reflector->invokeMethodGVP(entity,meth,valus);
	}
}

bool DataSourceInterface::startSession(void* details) {
	if(context==NULL) {
		context = getContext(details);
	}
}

bool DataSourceInterface::startSession() {
	if(context==NULL) {
		context = getContext(NULL);
	}
}

bool DataSourceInterface::endSession() {
	if(context!=NULL) {
		destroyContext(context);
		context = NULL;
	}
}
