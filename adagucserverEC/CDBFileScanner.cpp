#include "CDBFileScanner.h"
const char *CDBFileScanner::className="CDBFileScanner";

#define MAX_STR_LEN 8191
int CDBFileScanner::createDBUpdateTables(CPGSQLDB *DB,CDataSource *dataSource,int &removeNonExistingFiles){
  int status = 0;
  CT::string query;
  //First check and create all tables...
  for(size_t d=0;d<dataSource->cfgLayer->Dimension.size();d++){
    bool isTimeDim = false;
    CT::string dimName(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
    dimName.toLowerCase();
    if(dimName.equals("time"))isTimeDim=true;
    //How do we detect correctly wether this is a time dim?
    if(dimName.indexOf("time")!=-1)isTimeDim=true;
    
    //Create database tablenames
    CT::string tablename(dataSource->cfgLayer->DataBaseTable[0]->value.c_str());
    CServerParams::makeCorrectTableName(&tablename,&dimName);
    //Create column names
    CT::string tableColumns("path varchar (255)");
    if(isTimeDim==true){
      tableColumns.printconcat(", %s timestamp, dim%s int",dimName.c_str(),dimName.c_str());
    }else{
      tableColumns.printconcat(", %s real, dim%s int",dimName.c_str(),dimName.c_str());
    }
    // Test if the table exists , if not create the table
    CDBDebug("Check table %s ...\t",tablename.c_str());
    status = DB->checkTable(tablename.c_str(),tableColumns.c_str());
    //if(status == 0){CDBDebug("OK: Table is available");}
    if(status == 1){CDBError("\nFAIL: Table %s could not be created: %s",tablename.c_str(),tableColumns.c_str()); DB->close();return 1;  }
    if(status == 2){
      //removeExisting files can be set back to zero, because there are no files to remove (table is created)
      //note the int &removeNonExistingFiles as parameter of this function!
      //(Setting the value will have effect outside this function)
      removeNonExistingFiles=0;
      //CDBDebug("OK: Table %s created, (check for unavailable files is off);",tablename.c_str());
      
      //Create a index on these files:
      
      query.print("create index %s_idx on %s (%s)",tablename.c_str(),tablename.c_str(),dimName.c_str());
      CDBDebug("%s",query.c_str());
      if(DB->query(query.c_str())!=0){
        CDBError("Query %s failed",query.c_str());
        DB->close();
        return 1;
      }
      
      query.print("create index %s_pathidx on %s (path)",tablename.c_str(),tablename.c_str());
      CDBDebug("%s",query.c_str());
      if(DB->query(query.c_str())!=0){
        CDBError("Query %s failed",query.c_str());
        DB->close();
        return 1;
      }
    }
    
    
    
    if(removeNonExistingFiles==1){
      //The temporary table should always be dropped before filling.  
      //We will do a complete new update, so store everything in an new table
      //Later we will rename this table
      CT::string tablename_temp(&tablename);
      if(removeNonExistingFiles==1){
        tablename_temp.concat("_temp");
      }
      CDBDebug("Making empty temporary table %s ... ",tablename_temp.c_str());
      
      status=DB->checkTable(tablename_temp.c_str(),tableColumns.c_str());
      if(status == 1){CDBError("\nFAIL: Table %s could not be created: %s",tablename_temp.c_str(),tableColumns.c_str()); DB->close();return 1;  }
      if(status == 2){
        //OK
      }
      if(status==0){
        query.print("drop table %s",tablename_temp.c_str());
        CDBDebug("\n*** %s",query.c_str());
        if(DB->query(query.c_str())!=0){
          CDBError("Query %s failed",query.c_str());
          DB->close();
          return 1;
        }
        CDBDebug("Check table %s ... ",tablename_temp.c_str());
        status = DB->checkTable(tablename_temp.c_str(),tableColumns.c_str());
        if(status == 0){CDBDebug("OK: Table is available");}
        if(status == 1){CDBError("\nFAIL: Table %s could not be created: %s",tablename_temp.c_str(),tableColumns.c_str()); DB->close();return 1;  }
        if(status == 2){CDBDebug("OK: Table %s created",tablename_temp.c_str());}
      }
      //Create a index on these files:
      query.print("create index %s_idx on %s (%s)",tablename_temp.c_str(),tablename_temp.c_str(),dimName.c_str());
      CDBDebug("%s",query.c_str());
      DB->query(query.c_str());
      /*if(DB->query(query.c_str())!=0){
       *          CDBError("Query %s failed",query.c_str());
       *          DB->close();
       *          return 1;
    }*/
    }
    
  }
  return 0;
}



int CDBFileScanner::DBLoopFiles(CPGSQLDB *DB,CDataSource *dataSource,int removeNonExistingFiles,CDirReader *dirReader){
  CT::string query;
  CDFObject *cdfObject = NULL;
  int status = 0;
  try{
    //Loop dimensions and files
    CDBDebug("Checking files that are already in the database...");
    char ISOTime[MAX_STR_LEN+1];
    size_t numberOfFilesAddedFromDB=0;
    
    //Setup variables like tablenames and timedims for each dimension
    size_t numDims=dataSource->cfgLayer->Dimension.size();
    bool isTimeDim[numDims];
    CT::string dimNames[numDims];
    CT::string tableColumns[numDims];
    CT::string tablenames[numDims];
    CT::string tablenames_temp[numDims];
    for(size_t d=0;d<dataSource->cfgLayer->Dimension.size();d++){
      dimNames[d].copy(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
      dimNames[d].toLowerCase();
      isTimeDim[d]=false;
      if(dimNames[d].equals("time"))isTimeDim[d]=true;
                            //TODO: implement use of standardname? How do we detect correctly wether this is a time dim?
      if(dimNames[d].indexOf("time")!=-1)isTimeDim[d]=true;
         //Create column names
      tableColumns[d].copy("path varchar (255)");
      if(isTimeDim[d]==true){
        tableColumns[d].printconcat(", time timestamp, dimtime int");
      }else{
        tableColumns[d].printconcat(", %s real, dim%s int",dimNames[d].c_str(),dimNames[d].c_str());
      }
      //Create database tablenames
      tablenames[d].copy(dataSource->cfgLayer->DataBaseTable[0]->value.c_str());
      CServerParams::makeCorrectTableName(&(tablenames[d]),&(dimNames[d]));
      //Create temporary tablename
      tablenames_temp[d].copy(&(tablenames[d]));
      if(removeNonExistingFiles==1){
        tablenames_temp[d].concat("_temp");
      }
    }
    
    for(size_t j=0;j<dirReader->fileList.size();j++){
      //Loop through all configured dimensions.
      #ifdef CDATAREADER_DEBUG
      CDBDebug("Loop through all configured dimensions.");
        #endif
        for(size_t d=0;d<dataSource->cfgLayer->Dimension.size();d++){
          int fileExistsInDB=0;
          //If we are messing in the non-temporary table (e.g.removeNonExistingFiles==0)
          //we need to make a transaction to make sure a file is not added twice
          //If removeNonExistingFiles==1, we are using the temporary table
          //Which is already locked by a transaction 
          if(removeNonExistingFiles==0){
            #ifdef USEQUERYTRANSACTIONS                
            status = DB->query("BEGIN"); if(status!=0)throw(__LINE__);
            #endif          
          }
          // Check if file already resides in the database
          query.print("select path from %s where path = '%s' limit 1",
         tablenames[d].c_str(),
         dirReader->fileList[j]->fullName.c_str());
      
      CT::string *pathValues = DB->query_select(query.c_str(),0);
      if(pathValues == NULL){CDBError("Query failed");DB->close();throw(__LINE__);}
      // Does the file already reside in the DB?
      if(pathValues->count==1)
        fileExistsInDB=1; //YES file is already in the database
        else
          fileExistsInDB=0; // NO
          delete[] pathValues;
        
        //Insert the record corresponding to this file from the nontemporary table into the temporary table
          if(fileExistsInDB == 1&&removeNonExistingFiles==1){
            //The file resides already in the database, copy it into the new table with _temp
            CT::string mvRecordQuery;
            mvRecordQuery.print("INSERT INTO %s select path,%s,dim%s from %s where path = '%s'",
                                      tablenames_temp[d].c_str(),
                                      dimNames[d].c_str(),
                                      dimNames[d].c_str(),
                                      tablenames[d].c_str(),
                                      dirReader->fileList[j]->fullName.c_str()
      );
      //printf("%s\n",mvRecordQuery.c_str());
      if(j%1000==0){
        CDBDebug("Adding record %d/%d\t %s",
                 (int)j,
                 (int)dirReader->fileList.size(),
                 dirReader->fileList[j]->baseName.c_str());
      }
      if(DB->query(mvRecordQuery.c_str())!=0){CDBError("Query %s failed",mvRecordQuery.c_str());throw(__LINE__);}
      numberOfFilesAddedFromDB++;
          }
          
          //The file metadata does not already reside in the db.
          //Therefore we need to read information from it
          if(fileExistsInDB == 0){
            try{
              CDBDebug("Adding fileNo %d/%d %s\t %s",
                            (int)j,
                            (int)dirReader->fileList.size(),
                            dataSource->cfgLayer->Dimension[d]->attr.name.c_str(),
                            dirReader->fileList[j]->baseName.c_str());
      #ifdef CDATAREADER_DEBUG
      CDBDebug("Creating new CDFObject");
      #endif
      cdfObject = CDFObjectStore::getCDFObjectStore()->getCDFObject(dataSource,dirReader->fileList[j]->fullName.c_str());
      if(cdfObject == NULL)throw(__LINE__);
      
      //Open the file
      #ifdef CDATAREADER_DEBUG
      CDBDebug("Opening file %s",dirReader->fileList[j]->fullName.c_str());
      #endif
      
      status = cdfObject->open(dirReader->fileList[j]->fullName.c_str());
      if(status!=0){
        CDBError("Unable to open file '%s'",dirReader->fileList[j]->fullName.c_str());
        throw(__LINE__);
      }
      #ifdef CDATAREADER_DEBUG
      CDBDebug("File opened.");
      #endif
      
      if(status==0){
        CDF::Dimension * dimDim = cdfObject->getDimensionNE(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
        CDF::Variable *  dimVar = cdfObject->getVariableNE(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
        if(dimDim==NULL||dimVar==NULL){
          CDBError("In file %s",dirReader->fileList[j]->fullName.c_str());
          CDBError("For variable '%s' dimension '%s' not found",
                   dataSource->cfgLayer->Variable[0]->value.c_str(),
                   dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
          throw(__LINE__);
        }else{
          CDF::Attribute *dimUnits = dimVar->getAttributeNE("units");
          if(dimUnits==NULL){
            dimVar->setAttributeText("units","1");
            dimUnits = dimVar->getAttributeNE("units");
          }
          //if(dimVar->data==NULL){
            status = dimVar->readData(CDF_DOUBLE);if(status!=0){
              CDBError("Unable to read variable data for %s",dimVar->name.c_str());
              throw(__LINE__);
            }
            
            //}
            //Add other dim than time
            if(isTimeDim[d]==false){
              const double *dimValues=(double*)dimVar->data;
              for(size_t i=0;i<dimDim->length;i++){
                CT::string queryString;
                queryString.print("INSERT into %s VALUES ('%s','%f','%d')",
                                  tablenames_temp[d].c_str(),
                                  dirReader->fileList[j]->fullName.c_str(),
                                  double(dimValues[i]),
                                  int(i));
                status = DB->query(queryString.c_str()); if(status!=0)throw(__LINE__);
                if(removeNonExistingFiles==1){
                  //We are adding the query above to the temporay table if removeNonExistingFiles==1;
                  //Lets add it also to the real table for convenience
                  //Later this table will be dropped, but it will remain more up to date during scanning this way.
                  CT::string queryString;
                  queryString.print("INSERT into %s VALUES ('%s','%f','%d')",
                                    tablenames_temp[d].c_str(),
                                    dirReader->fileList[j]->fullName.c_str(),
                                    double(dimValues[i]),
                                    int(i));
                  CDBDebug("%s",queryString.c_str());
                  status = DB->query(queryString.c_str()); if(status!=0)throw(__LINE__);
                }
              }
            }
            //Add time dim:
            if(isTimeDim[d]==true){
              //TODO read standard_name and check for value time, this ensures that it is a time dim.
              //CDBDebug("Treating %s as a time dimension",dimVar->name.c_str());
              const double *dtimes=(double*)dimVar->data;
              CADAGUC_time *ADTime  = NULL;
              try{ADTime = new CADAGUC_time((char*)dimUnits->data);}catch(int e){delete ADTime;ADTime=NULL;throw(__LINE__);}
              for(size_t i=0;i<dimDim->length;i++){
                if(dtimes[i]!=NC_FILL_DOUBLE){
                  ADTime->PrintISOTime(ISOTime,MAX_STR_LEN,dtimes[i]);
                  status = 0;//TODO make PrintISOTime return a 0 if succeeded
                  if(status == 0){
                    ISOTime[19]='Z';ISOTime[20]='\0';
          //printf("%s\n", ISOTime);
          CT::string queryString;
          queryString.print("INSERT into %s VALUES ('%s','%s','%d')",
                            tablenames_temp[d].c_str(),
                            dirReader->fileList[j]->fullName.c_str(),
                            ISOTime,
                            int(i));
          status = DB->query(queryString.c_str()); if(status!=0)throw(__LINE__);
          if(removeNonExistingFiles==1){
            //We are adding the query above to the temporay table if removeNonExistingFiles==1;
            //Lets add it also to the real table for convenience
            //Later this table will be dropped, but it will remain more up to date during scanning this way.
            CT::string queryString;
            queryString.print("INSERT into %s VALUES ('%s','%s','%d')",
                              tablenames[d].c_str(),
                              dirReader->fileList[j]->fullName.c_str(),
                              ISOTime,
                              int(i));
            status = DB->query(queryString.c_str()); if(status!=0)throw(__LINE__);
          }
                  }else {
                    CDBError("Time attribute invalid in [%s]",dirReader->fileList[j]->fullName.c_str());
                  }
                }
              }
              delete ADTime;
            }
      }
            }
            //delete cdfObject;cdfObject=NULL;
            cdfObject=CDFObjectStore::getCDFObjectStore()->deleteCDFObject(&cdfObject);
          }catch(int linenr){
            CDBError("File open exception in DBLoopFiles at line %d",linenr);
            CDBError(" *** SKIPPING FILE %s ***",dirReader->fileList[j]->baseName.c_str());
            //Close cdfObject. this is only needed if an exception occurs, otherwise it does nothing...
            //delete cdfObject;cdfObject=NULL;
            cdfObject=CDFObjectStore::getCDFObjectStore()->deleteCDFObject(&cdfObject);
          }
        }
        //If we are messing in the non-temporary table (e.g.removeNonExistingFiles==0)
        //we need to make a transaction to make sure a file is not added twice
        //If removeNonExistingFiles==1, we are using the temporary table
        //Which is already locked by a transaction 
        if(removeNonExistingFiles==0){
          //CDBDebug("COMMIT");
          #ifdef USEQUERYTRANSACTIONS                
          status = DB->query("COMMIT"); if(status!=0)throw(__LINE__);
          #endif          
        }
        
    }
    
  }
  if(status != 0){CDBError(DB->getError());throw(__LINE__);}
  if(numberOfFilesAddedFromDB!=0){CDBDebug("%d files not scanned, they were already in the database",numberOfFilesAddedFromDB);}
  //    CDBDebug("%d files scanned from disk",dirReader->fileList.size());
  
}
catch(int linenr){
  #ifdef USEQUERYTRANSACTIONS                    
  DB->query("COMMIT"); 
  #endif    
  CDBError("Exception in DBLoopFiles at line %d",linenr);
  cdfObject=CDFObjectStore::getCDFObjectStore()->deleteCDFObject(&cdfObject);
  return 1;
}


//delete cdfObject;cdfObject=NULL;
cdfObject=CDFObjectStore::getCDFObjectStore()->deleteCDFObject(&cdfObject);
return 0;
}



int CDBFileScanner::updatedb(const char *pszDBParams, CDataSource *dataSource,CT::string *_tailPath,CT::string *_layerPathToScan){
  
  if(dataSource->dLayerType!=CConfigReaderLayerTypeDataBase)return 0;
            
            //We only need to update the provided path in layerPathToScan. We will simply ignore the other directories
  if(_layerPathToScan!=NULL){
    if(_layerPathToScan->length()!=0){
      CT::string layerPath,layerPathToScan;
      layerPath.copy(dataSource->cfgLayer->FilePath[0]->value.c_str());
      layerPathToScan.copy(_layerPathToScan);
      
      CDirReader::makeCleanPath(&layerPath);
      CDirReader::makeCleanPath(&layerPathToScan);
      
      //If this is another directory we will simply ignore it.
      if(layerPath.equals(&layerPathToScan)==false){
        CDBError("Skipping %s==%s\n",layerPath.c_str(),layerPathToScan.c_str());
        return 0;
      }
    }
  }
  // This variable enables the query to remove files that no longer exist in the filesystem
  int removeNonExistingFiles=1;
  
  int status;
  CPGSQLDB DB;
  
  //Copy tailpath (can be provided to scan only certain subdirs)
  
  CT::string tailPath(_tailPath);
  
  CDirReader::makeCleanPath(&tailPath);
  
  //if tailpath is defined than removeNonExistingFiles must be zero
  if(tailPath.length()>0)removeNonExistingFiles=0;
            
            
            //temp = new CDirReader();
  //temp->makeCleanPath(&FilePath);
  //delete temp;
  
  CDirReader dirReader;
  
  CDBDebug("\n*** Starting update layer \"%s\"",dataSource->cfgLayer->Name[0]->value.c_str());
  
  if(searchFileNames(dataSource,&dirReader,&tailPath)!=0)return 0;
            
            if(dirReader.fileList.size()==0)return 0;
            
            /*----------- Connect to DB --------------*/
            CDBDebug("Connecting to DB ...\t");
  status = DB.connect(pszDBParams);if(status!=0){
    CDBError("FAILED: Unable to connect to the database with parameters: [%s]",pszDBParams);
    return 1;
  }
  
  try{ 
    //First check and create all tables...
    status = createDBUpdateTables(&DB,dataSource,removeNonExistingFiles);
    if(status != 0 )throw(__LINE__);
               
               //CDBDebug("removeNonExistingFiles = %d\n",removeNonExistingFiles);
    //We need to do a transaction if we want to remove files from the existing table
    if(removeNonExistingFiles==1){
      //CDBDebug("BEGIN");
      #ifdef USEQUERYTRANSACTIONS      
      status = DB.query("BEGIN"); if(status!=0)throw(__LINE__);
      #endif      
    }
    
    
    //Loop Through all files
    status = DBLoopFiles(&DB,dataSource,removeNonExistingFiles,&dirReader);
    if(status != 0 )throw(__LINE__);
                                              
                                              //In case of a complete update, the data is written in a temporary table
    //Rename the table to the correct one (remove _temp)
    if(removeNonExistingFiles==1){
      for(size_t d=0;d<dataSource->cfgLayer->Dimension.size();d++){
        CT::string dimName(dataSource->cfgLayer->Dimension[d]->attr.name.c_str());
        CT::string tablename(dataSource->cfgLayer->DataBaseTable[0]->value.c_str());
        CServerParams::makeCorrectTableName(&tablename,&dimName);
        CDBDebug("Renaming temporary table... %s",tablename.c_str());
        CT::string query;
        //Drop old table
        query.print("drop table %s",tablename.c_str());
        if(DB.query(query.c_str())!=0){CDBError("Query %s failed",query.c_str());DB.close();throw(__LINE__);}
        //Rename new table to old table name
        query.print("alter table %s_temp rename to %s",tablename.c_str(),tablename.c_str());
        if(DB.query(query.c_str())!=0){CDBError("Query %s failed",query.c_str());DB.close();throw(__LINE__);}
        if(status!=0){throw(__LINE__);}
      }
    }
    
  }
  catch(int linenr){
    CDBError("Exception in updatedb at line %d",linenr);
    #ifdef USEQUERYTRANSACTIONS                    
    if(removeNonExistingFiles==1)status = DB.query("COMMIT");
                                              #endif    
    status = DB.close();return 1;
  }
  // Close DB
  //CDBDebug("COMMIT");
  #ifdef USEQUERYTRANSACTIONS                  
  if(removeNonExistingFiles==1)status = DB.query("COMMIT");
                                              #endif  
  status = DB.close();if(status!=0)return 1;
  
  CDBDebug("*** Finished");
  //printStatus("OK","HOi %s","Maarten");
  return 0;
}
int CDBFileScanner::searchFileNames(CDataSource *dataSource,CDirReader *dirReader,CT::string *tailPath){
  CT::string filePath=dataSource->cfgLayer->FilePath[0]->value.c_str();
  if(tailPath!=NULL)filePath.concat(tailPath);
                                              
                                              
                                              if(filePath.lastIndexOf(".nc")>0||filePath.indexOf("http://")>=0||filePath.indexOf("https://")>=0){
                                                CFileObject * fileObject = new CFileObject();
                                                fileObject->fullName.copy(&filePath);
                                                fileObject->baseName.copy(&filePath);
                                                fileObject->isDir=false;
                                                dirReader->fileList.push_back(fileObject);
                                              }else{
                                                
                                                dirReader->makeCleanPath(&filePath);
                                                
                                                try{
                                                  CT::string fileFilterExpr("\\.nc");
                                                  
                                                  if(dataSource->cfgLayer->FilePath[0]->attr.filter.c_str()!=NULL){
                                                    
                                                    fileFilterExpr.copy(dataSource->cfgLayer->FilePath[0]->attr.filter.c_str());
                                                    
                                                  }
                                                  CDBDebug("Reading directory %s with filter %s",filePath.c_str(),fileFilterExpr.c_str()); 
                                                  dirReader->listDirRecursive(filePath.c_str(),fileFilterExpr.c_str());
                                                }catch(const char *msg){
                                                  CDBDebug("Directory %s does not exist, silently skipping...",filePath.c_str());
            return 1;
                                                }
                                              }
                                              //#ifdef CDATAREADER_DEBUG     
                                              CDBDebug("Found %d file(s) in directory",int(dirReader->fileList.size()));
                                              //#endif  
                                              return 0;
}
