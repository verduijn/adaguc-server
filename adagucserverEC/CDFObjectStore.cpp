/******************************************************************************
 * 
 * Project:  ADAGUC Server
 * Purpose:  ADAGUC OGC Server
 * Author:   Maarten Plieger, plieger "at" knmi.nl
 * Date:     2013-06-01
 *
 ******************************************************************************
 *
 * Copyright 2013, Royal Netherlands Meteorological Institute (KNMI)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 ******************************************************************************/

#include "CDFObjectStore.h"
const char *CDFObjectStore::className="CDFObjectStore";
#include "CConvertASCAT.h"
#include "CConvertUGRIDMesh.h"
#include "CConvertADAGUCVector.h"
#include "CConvertADAGUCPoint.h"
#include "CConvertCurvilinear.h"
#include "CDataReader.h"
//#define CDFOBJECTSTORE_DEBUG
#define MAX_OPEN_FILES 200
extern CDFObjectStore cdfObjectStore;
CDFObjectStore cdfObjectStore;
bool EXTRACT_HDF_NC_VERBOSE = false;
/**
 * Get a CDFReader based on information in the datasource. In the Layer element this can be configured with <DataReader>HDF5</DataReader>
 * @param dataSource The configured datasource or NULL pointer. NULL pointer defaults to a NetCDF/OPeNDAP reader
 */
CDFReader *CDFObjectStore::getCDFReader(CDataSource *dataSource,const char *fileName){
  //Do we have a datareader defined in the configuration file?
  //if(cdfReader !=NULL){delete cdfReader;cdfReader = NULL;}
  CDFReader *cdfReader = NULL;
  
  // CDFObject *cdfObject=dataSource->dataObject[0]->cdfObject;
  if(dataSource!=NULL){
    if(dataSource->cfgLayer->DataReader.size()>0){
      if(dataSource->cfgLayer->DataReader[0]->value.equals("HDF5")){
        #ifdef CDFOBJECTSTORE_DEBUG
        CDBDebug("Creating HDF5 reader");
        #endif
        cdfReader = new CDFHDF5Reader();
        CDFHDF5Reader * hdf5Reader = (CDFHDF5Reader*)cdfReader;
        hdf5Reader->enableKNMIHDF5toCFConversion();
      }
    }else{
      cdfReader=getCDFReader(fileName);
    }
  }
  //Defaults to the netcdf reader
  if(cdfReader==NULL){
    #ifdef CDFOBJECTSTORE_DEBUG
    CDBDebug("Creating NetCDF reader");
    #endif
    cdfReader = new CDFNetCDFReader();
  }
  return cdfReader;
}


/**
 * Get a CDFReader based on fileName information, currently based on extension.
 * @param fileName The fileName
 * @return The CDFReader
 */
CDFReader *CDFObjectStore::getCDFReader(const char *fileName){

  CDFReader *cdfReader = NULL;
  if(fileName!=NULL){
    CT::string name=fileName;
    int a=name.indexOf(".h5");
    if(a!=-1){
      if(a==int(name.length())-3){
        if(EXTRACT_HDF_NC_VERBOSE){
          CDBDebug("Creating HDF5 reader");
        }
        cdfReader = new CDFHDF5Reader();
        CDFHDF5Reader * hdf5Reader = (CDFHDF5Reader*)cdfReader;
        hdf5Reader->enableKNMIHDF5toCFConversion();
      }
    }
    if(cdfReader==NULL){
      a=name.indexOf(".he5");
      if(a!=-1){
        if(a==int(name.length())-4){
          if(EXTRACT_HDF_NC_VERBOSE){
            CDBDebug("Creating HDF EOS 5 reader");
          }
          cdfReader = new CDFHDF5Reader();
        }
      }
    }
    if(cdfReader==NULL){
      a=name.indexOf(".hdf");
      if(a!=-1){
        if(a==int(name.length())-4){
          if(EXTRACT_HDF_NC_VERBOSE){
            CDBDebug("Creating HDF reader");
          }
          cdfReader = new CDFHDF5Reader();
        }
      }
    }
  }
  //Defaults to the netcdf reader
  if(cdfReader==NULL){
    if(EXTRACT_HDF_NC_VERBOSE){
      CDBDebug("Creating NetCDF reader %s",fileName);
    }
    cdfReader = new CDFNetCDFReader();
    //((CDFNetCDFReader*)cdfReader)->enableLonWarp(true);
    
  }
  return cdfReader;
}

CDFObject *CDFObjectStore::getCDFObjectHeader(CServerParams *srvParams,const char *fileName){
    if(srvParams == NULL){
      CDBError("srvParams == NULL");
      return NULL;
    }

    return getCDFObject(NULL,srvParams,fileName);
}


/**
* Get a CDFObject based with opened and configured CDF reader for a filename/OPeNDAP url and a dataSource.
* @param dataSource The configured datasource or NULL pointer. NULL pointer defaults to a NetCDF/OPeNDAP reader
* @param fileName The filename to read.
*/
CDFObject *CDFObjectStore::getCDFObject(CDataSource *dataSource,const char *fileName){
  return getCDFObject(dataSource,NULL,fileName);
}
  

CDFObject *CDFObjectStore::getCDFObject(CDataSource *dataSource,CServerParams *srvParams,const char *fileName){
  CT::string uniqueIDForFile = fileName;
  

  for(size_t j=0;j<fileNames.size();j++){
    if(fileNames[j]->equals(uniqueIDForFile.c_str())){
      #ifdef CDFOBJECTSTORE_DEBUG                          
      CDBDebug("Found CDFObject with filename %s",uniqueIDForFile.c_str());
      #endif            
      return cdfObjects[j];
    }
  }
  if(cdfObjects.size()>MAX_OPEN_FILES){
    deleteCDFObject(&cdfObjects[0]);
  }
  #ifdef CDFOBJECTSTORE_DEBUG              
  CDBDebug("Creating CDFObject with id %s",uniqueIDForFile.c_str());
  #endif      
 
  
  //Open the object.
  #ifdef CDFOBJECTSTORE_DEBUG           
  CDBDebug("Opening %s",fileName);
  #endif
  
  //Open header
  
  

  
  

  
  const char *fileLocationToOpen = fileName;
 
  if(EXTRACT_HDF_NC_VERBOSE){
    CDBDebug("Opening from file: %s",fileLocationToOpen );
  }
  
  
  
   //CDFObject not found: Create one
  CDFObject *cdfObject = new CDFObject();
  CDFReader *cdfReader = NULL;
  
  if(dataSource!=NULL){
    cdfReader = CDFObjectStore::getCDFReader(dataSource,fileLocationToOpen);
  }else{
    //Get a reader based on file extension
    cdfReader = CDFObjectStore::getCDFReader(fileLocationToOpen);
  }
  
  if(cdfReader==NULL){
    if(dataSource!=NULL){
      CDBError("Unable to get a reader for source %s",dataSource->cfgLayer->Name[0]->value.c_str());
    }
    throw(1);
  }
  
 
  CDFCache* cdfCache = NULL;

  if(srvParams!=NULL){
 
    
    CT::string cacheDir = srvParams->cfg->TempDir[0]->attr.value.c_str();
    //srvParams->getCacheDirectory(&cacheDir);
    if(cacheDir.length()>0){
      if(srvParams->isAutoResourceCacheEnabled()){
        cdfCache = new CDFCache(cacheDir);
        cdfReader->cdfCache = cdfCache;
      }
    }

  }
  
  
  cdfObject->attachCDFReader(cdfReader);
  
  int status = cdfObject->open(fileLocationToOpen);
   //CDBDebug("opened");
  if(status!=0){
    //TODO in case of basic/digest authentication, username and password is currently also listed....
    CDBError("Unable to open file '%s'",fileLocationToOpen);
    delete cdfObject;
    delete cdfReader;
    delete cdfCache;
    return NULL;
  }
  
 
  
  
   
  //CDBDebug("PUSHING %s",uniqueIDForFile.c_str());
  //Push everything into the store
  fileNames.push_back(new CT::string(uniqueIDForFile.c_str()));
  cdfObjects.push_back(cdfObject);
  cdfReaders.push_back(cdfReader);
  
  

  bool level2CompatMode = false;
  
  if(!level2CompatMode)if(CConvertUGRIDMesh::convertUGRIDMeshHeader(cdfObject)==0){level2CompatMode=true;};
  
  if(!level2CompatMode)if(CConvertASCAT::convertASCATHeader(cdfObject)==0){level2CompatMode=true;};
   
  if(!level2CompatMode)if(CConvertADAGUCVector::convertADAGUCVectorHeader(cdfObject)==0){level2CompatMode=true;};
   
  if(!level2CompatMode)if(CConvertADAGUCPoint::convertADAGUCPointHeader(cdfObject)==0){level2CompatMode=true;};
   
  if(!level2CompatMode)if(CConvertCurvilinear::convertCurvilinearHeader(cdfObject,srvParams)==0){level2CompatMode=true;};
    
  
  return cdfObject;
}
CDFObjectStore *CDFObjectStore::getCDFObjectStore(){return &cdfObjectStore;};


CDFObject *CDFObjectStore::deleteCDFObject(CDFObject **cdfObject){
  CDBDebug("Deleting CDFObject");
  for(size_t j=0;j<cdfObjects.size();j++){
    if(cdfObjects[j]==(*cdfObject)){
      //CDBDebug("Closing %s",fileNames[j]->c_str());
      delete cdfObjects[j];cdfObjects[j]=NULL;(*cdfObject)=NULL;
      delete fileNames[j]; fileNames[j] = NULL;
      delete cdfReaders[j]->cdfCache;cdfReaders[j]->cdfCache = NULL;
      delete cdfReaders[j];cdfReaders[j] = NULL;
      cdfReaders.erase(cdfReaders.begin()+j);
      cdfObjects.erase(cdfObjects.begin()+j);
      fileNames.erase(fileNames.begin()+j);
      return NULL;
    }
  }
  
  return NULL;
}

/**
 * Clean the CDFObject store and throw away all readers and objects
 */
void CDFObjectStore::clear(){
  for(size_t j=0;j<fileNames.size();j++){
    delete fileNames[j]; fileNames[j] = NULL;
    delete cdfObjects[j];cdfObjects[j] = NULL;
    delete cdfReaders[j]->cdfCache;cdfReaders[j]->cdfCache = NULL;
    delete cdfReaders[j];cdfReaders[j] = NULL;
  }
  fileNames.clear();
  cdfReaders.clear();
  cdfObjects.clear();
}


CT::StackList<CT::string> CDFObjectStore::getListOfVisualizableVariables(CDFObject *cdfObject){
  CT::StackList<CT::string> variableList;  
  
  if(cdfObject!=NULL){
    for(size_t j=0;j<cdfObject->variables.size();j++){
      if(cdfObject->variables[j]->dimensionlinks.size()>=2){
        if(cdfObject->variables[j]->getAttributeNE("ADAGUC_SKIP")==NULL){
          if(!cdfObject->variables[j]->name.equals("lon")&&
            !cdfObject->variables[j]->name.equals("lat")&&
            !cdfObject->variables[j]->name.equals("lon_bounds")&&
            !cdfObject->variables[j]->name.equals("lat_bounds")&&
            !cdfObject->variables[j]->name.equals("time_bounds")&&
            !cdfObject->variables[j]->name.equals("lon_bnds")&&
            !cdfObject->variables[j]->name.equals("lat_bnds")&&
            !cdfObject->variables[j]->name.equals("time_bnds")&&
            !cdfObject->variables[j]->name.equals("time")){
              variableList.push_back(cdfObject->variables[j]->name.c_str());
          }
        }
      }
    }
  }
  return variableList;
}

