#include "symbol_table.h"

sym_table gst;
struct_sym_table struct_gst;
map<sym_table*, sym_table*> parent_table;
map<struct_sym_table*, struct_sym_table*> struct_parent_table;
map<string, pair< string,vector<string> > > func_arg;
int struct_offset;
sym_table* curr_table;
sym_table* curr_structure;
struct_sym_table *curr_struct_table;
stack<int> Goffset, Loffset, blockSz;
typ_table typ_gst;
map<typ_table*, typ_table*> typ_parent_table;
typ_table* curr_typ;
int max_size = 0;
int param_offset = -4;
int struct_count = 1;
int avl=0;
extern int isArray;
extern vector<int> array_dims;
extern map<string, int> func_usage_map;
extern int dump_sym_table;
map<string, pair<string, int>> globaldecl;
int blockCnt = 1;

// initialize base symbol table
void symTable_init(){
	Goffset.push(0);
	Loffset.push(0);
	blockSz.push(0);
	parent_table.insert(make_pair(&gst, nullptr));
	struct_parent_table.insert(make_pair(&struct_gst, nullptr));
	curr_table = &gst;
	curr_struct_table = &struct_gst;
	curr_typ = &typ_gst;
	insertKeywords();
}

// constructor for symbol table entry
sym_entry* createEntry(string type, int size, bool init, int offset, sym_table* ptr){
	sym_entry* new_sym = new sym_entry();
	new_sym->type = type;
	new_sym->size = size;
	new_sym->init = init;
	new_sym->offset = offset;
	new_sym->entry = ptr;
	return new_sym;
}

// create new symbol tables for new scopes
void makeSymbolTable(string name, string f_type, int offset_flag){
	if(!avl){
		sym_table* new_table = new sym_table;
		struct_sym_table* new_struct_table = new struct_sym_table;
		typ_table* new_typ = new typ_table;

		if(f_type != "") insertSymbol(*curr_table, name , "FUNC_" + f_type , 0 , 1, new_table);
		else{
			insertSymbol(*curr_table, name , "Block",0,1, new_table);
			blockCnt++;
		}

		Goffset.push(0);
		if(offset_flag)blockSz.push(0);
		parent_table.insert(make_pair(new_table, curr_table));
		struct_parent_table.insert(make_pair(new_struct_table, curr_struct_table));
		typ_parent_table.insert(make_pair(new_typ, curr_typ));

		curr_table = new_table;
		curr_struct_table = new_struct_table;
		curr_typ = new_typ;
	}
	else{
		avl = 0;
		(*parent_table[curr_table]).erase("dummyF_name");
		(*parent_table[curr_table]).insert(make_pair(name, createEntry("FUNC_"+f_type,0,1,Loffset.top(), curr_table)));
		Loffset.pop();
	}
}

// remove func prototype from symbol table
void removeFuncProto(){
	avl = 0;
	clear_paramoffset();
	updSymbolTable("dummyF_name",1);
	parent_table.erase((*curr_table)["dummyF_name"]->entry);
	(*curr_table).erase("dummyF_name");
	Loffset.pop();
}

// update current symbol table and set parent as current (end of scope)
void updSymbolTable(string id, int offset_flag){
	int temp = Goffset.top();
	Goffset.pop();
	Goffset.top()+=temp;

	curr_table = parent_table[curr_table];
	curr_struct_table = struct_parent_table[curr_struct_table];
	curr_typ = typ_parent_table[curr_typ];

	sym_entry* entry = lookup(id);
	if(entry) entry->size = blockSz.top();

	if(offset_flag){
		temp = blockSz.top();
		blockSz.pop();
		blockSz.top()+=temp;
	}
}

// look up currently visible symbol table entry that corresponds to the id
sym_entry* lookup(string id){
	sym_table* temp = curr_table;
	while(temp){
		if((*temp).find(id)!=(*temp).end()) return (*temp)[id];
		temp = parent_table[temp];
	}
	return nullptr;
}

// look up function prototype parameter list
string funcProtoLookup(string id){
	if(func_arg.find(id)!= func_arg.end())return func_arg[id].first;
	else return "";
}

// find total size of local variables a function
int func_local_size(string name){
	return gst[name]->size;
}

// look up for a symbol in current symbol table only (only the current scope)
sym_entry* currLookup(string id){
	if((*curr_table).find(id)==(*curr_table).end()) return nullptr;
	return (*curr_table)[id];
}

// insert keywords into global symbol table
void insertKeywords(){
	vector<string> key_words = {"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","int","long","register","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while"}; 
	vector<string> op = {"...",">>=","<<=","+=","-=","*=","/=","%=","&=","^=","|=",">>","<<","++","--","->","&&","||","<=",">=","==","!=",";","{","<%","}","%>",",",":","=","(",")","[","<:","]",":>",".","&","!","~","-","+","*","/","%","<",">","^","|","?"};

	for(auto h:key_words){
		insertSymbol(*curr_table, h, "keyword", 8, 1, nullptr);
	}
	for(auto h:op){
		insertSymbol(*curr_table, h, "operator", 8, 1, nullptr);
	}
	
	// important io functions
	vector<string> type = {"char*", "..."};
	insert_imp_func("printf", type, "int");
	insert_imp_func("scanf", type, "int");

	// dynamic allocation functions
	type = {"int"};
	insert_imp_func("malloc", type, "void*");
	type = {"int", "int"};
	insert_imp_func("calloc", type, "void*");
	type = {"void*"};
	insert_imp_func("free", type, "void");

	// file io functions
	type = {"char*", "char*"};
	insert_imp_func("fopen", type, "FILE*");
	type = {"char*", "FILE*"};
	insert_imp_func("fputs", type, "int");
	type = {"char*", "int", "FILE*"};
	insert_imp_func("fgets", type, "int");
	type = {"FILE*"};
	insert_imp_func("fclose", type, "int");
	type = {"FILE*", "char*", "..."};
	insert_imp_func("fprintf", type, "int");
	type = {"FILE*", "char*", "..."};
	insert_imp_func("fscanf", type, "int");
	type = {"FILE*"};
	insert_imp_func("fgetc", type, "char");
	type = {"char", "FILE*"};
	insert_imp_func("fputc", type, "char");

	// string Functions
	type = {"char*"};
	insert_imp_func("strlen", type, "int");
	type = {"char*", "char*"};
	insert_imp_func("strcmp", type, "int");
	type = {"char*", "char*", "int"};
	insert_imp_func("strncmp", type, "int");
	type = {"char*", "char*"};
	insert_imp_func("strcpy", type, "char*");
	insert_imp_func("strcat", type, "char*");
}

// helper function to insert library functions in symbol table
void insert_imp_func(string func_name, vector<string> type, string ret_type){
	insertSymbol(*curr_table, func_name, "FUNC_"+ret_type, 4, 0, nullptr);
	func_arg.insert({func_name, make_pair("FUNC_"+ret_type, type)});
	func_usage_map.insert({func_name, 0});
}

// find the type of a symbol
string getType(string id){
	sym_entry* entry = lookup(id);
	string ret = "";
	if(entry) ret += entry->type;
	return ret;
}

// construct struct/ union table
void createStructTable(){
	sym_table* new_table = new sym_table;
	curr_structure = new_table;
	struct_offset = 0;
}

// insert struct/ union members (attributes) in symbol table
int insertStructAttr(string attr, string type, int size, bool init){  
	if((*curr_structure).find(attr)==(*curr_structure).end()){
		blockSz.top()+=size;
		Goffset.top()+=size;
		max_size = max(max_size, size);
		(*curr_structure).insert(make_pair(attr, createEntry(type,size,init, struct_offset, nullptr)));
		if(type[type.length()-1] == '*' && !array_dims.empty()){
			vector<int> temp;
			int curr = 1;
			for(int i = array_dims.size()-1; i>=1; i--){
				curr*=array_dims[i];
				temp.push_back(curr);
			}
			reverse(temp.begin(), temp.end());
			(*curr_structure)[attr]->array_dims = temp;
			if(isArray){
				(*curr_structure)[attr]->isArray = 1;
				isArray = 0;
			}
			array_dims.clear();
		}
		struct_offset += size;
		return 1;
	}
	return 0;
}

// print a struct/ union table into a csv file
int printStructTable(string struct_name){
	if((*curr_struct_table).find(struct_name)==(*curr_struct_table).end()){
		struct_parent_table.insert(make_pair(curr_struct_table, nullptr));
		if(struct_name.substr(0, 6) == "struct")(*curr_struct_table).insert(make_pair(struct_name, make_pair(struct_offset,curr_structure)));
		else{
			(*curr_struct_table).insert(make_pair(struct_name, make_pair(max_size,curr_structure)));
			for(auto it: *curr_structure){
				it.second->offset = 0;
			}
		}
		max_size = 0;
		printSymbolTable(curr_structure, struct_name + "_" + to_string(struct_count)+".csv");  // prints structre symbol table
		struct_count++;
		return 1;
	}
	return 0;
}

// used when accessing member of a struct/ union
sym_entry* retTypeAttrEntry(string struct_name, string id, string struct_var){
	struct_sym_table* temp = curr_struct_table;
	while((*temp).find(struct_name) == (*temp).end()){
		temp = struct_parent_table[temp];
	}
	sym_table* table = (*temp)[struct_name].second;
	sym_entry* struct_entry = lookup(struct_var);
	sym_entry* t = new sym_entry;
	t->type = ((*table)[id])->type;
	t->size = ((*table)[id])->size;
	t->offset = ((*table)[id])->offset;
	t->isArray = ((*table)[id])->isArray;
	t->array_dims = ((*table)[id])->array_dims;
	t->addr_descriptor = ((*table)[id])->addr_descriptor;
	t->next_use = -1;
	t->heap_mem = 0;
	t->is_derefer = 0;
	return t;
}

// look up the type of a struct member
string StructAttrType(string struct_name, string id){
	struct_sym_table* temp = curr_struct_table;
	while((*temp).find(struct_name) == (*temp).end()){
		temp = struct_parent_table[temp];
	}
	sym_table* table = (*temp)[struct_name].second;
	return ((*table)[id]->type);
}

// look up any user defined type (structs or unions)
int typeLookup(string struct_name){
	struct_sym_table* temp = curr_struct_table;
	while(temp){
		if((*temp).find(struct_name)!=(*temp).end()) return 1;
		temp = struct_parent_table[temp];
	}
	return 0;
}

// look up any user defined type (structs or unions) only in the current symbol table (current scope)
int currTypeLookup(string struct_name){
	struct_sym_table* temp = curr_struct_table;
	if((*temp).find(struct_name)!=(*temp).end()) return 1;
	return 0;
}

// search for any member of a user defined type
int findTypeAttr(string struct_name, string id){
	struct_sym_table* temp = curr_struct_table;
	while(temp){
		if((*temp).find(struct_name)!=(*temp).end()){
			if((*((*temp)[struct_name].second)).find(id)!=(*((*temp)[struct_name].second)).end()) return 1; // found the attr
			else return 0; // attr id not present
		}
		temp = struct_parent_table[temp];
	}
	return -1;	// struct (or union) not present
	
}

// initialize creation of a function table
void createParamList(){
	Loffset.push(Goffset.top());
	makeSymbolTable("dummyF_name", "",1);
	avl = 1;
}

// create a symbol table entry and put it in the given symbol table
void insertSymbol(sym_table& table, string id, string type, int size, bool is_init, sym_table* ptr){
	table.insert(make_pair(id, createEntry(type, size, is_init, blockSz.top(), ptr)));
	if(type[type.length()-1] == '*' && !array_dims.empty()){
		vector<int> temp;
		int curr = 1;
		for(int i = array_dims.size()-1; i>=1; i--){
			curr*=array_dims[i];
			temp.push_back(curr);
		}
		reverse(temp.begin(), temp.end());
		table[id]->array_dims = temp;
		if(isArray){
			table[id]->isArray = 1;
			isArray = 0;
		}
		array_dims.clear();
	}
	blockSz.top()+=size;
	Goffset.top()+=size;
}

// insert alternate typedef names into the symbol table with the corresponding type it represents
void insertTypedef(sym_table& table, string id, string type, int size, bool is_init, sym_table* ptr){
	table.insert(make_pair(id, createEntry(type, size, is_init, blockSz.top(), ptr)));
	if(type[type.length()-1] == '*' && !array_dims.empty()){
		vector<int> temp;
		int curr = 1;
		for(int i = array_dims.size()-1; i>=1; i--){
			curr*=array_dims[i];
			temp.push_back(curr);
		}
		reverse(temp.begin(), temp.end());
		table[id]->array_dims = temp;
		array_dims.clear();
	}
	table[id]->storage_class = "typedef";
	blockSz.top()+=size;
	Goffset.top()+=size;
}

// insert function parameters into the symbol table of the function
void paramInsert(sym_table& table, string id, string type, int size, bool is_init, sym_table* ptr){
	table.insert(make_pair(id, createEntry(type, size, is_init, param_offset-size, ptr)));
	if(type[type.length()-1] == '*' && !array_dims.empty()){
		size = 4;
		vector<int> temp;
		int curr = 1;
		for(int i = array_dims.size()-1; i>=1; i--){
			curr*=array_dims[i];
			temp.push_back(curr);
		}
		reverse(temp.begin(), temp.end());
		table[id]->array_dims = temp;
		table[id]->offset = param_offset - size;
		array_dims.clear();
	}
	param_offset-=size;
}

// reset parameter offset global variable before insertion
void clear_paramoffset(){
	param_offset = -4;
}

// return the list of function arguments
vector<string> getFuncArgs(string id){
	vector<string> temp;
	temp.push_back("#NO_FUNC");
	if(func_arg.find(id) != func_arg.end()) return func_arg[id].second;
	else return temp;
}

// return the return type of the required function
string getFuncType(string id){
	if(func_arg.find(id) != func_arg.end()) return func_arg[id].first;
	return "";
}

// update the init field of an entry
void updInit(string id){
	sym_entry* entry = lookup(id);
	if(entry) entry->init = true;
}

// update the argument list and return type of a function
void insertFuncArg(string &func, vector<string> &arg, string &tp){
	func_arg.insert(make_pair(func, make_pair(string("FUNC_" +tp),arg)));
}

// look up a specific user defined type in that is currently in scope
string lookupType(string a){
	typ_table* temp = curr_typ;
	while(temp){
		if((*temp).find(a)==(*temp).end()) return (*temp)[a];
		temp = typ_parent_table[temp];
	}
	return "";
}

// set global variables
void setGlobal(){
	for(auto &it: gst){
		if(it.second->type.substr(0,2) == "in" || it.second->type.substr(0,2)=="ch"){
			it.second->is_global = 1;
			globaldecl.insert(make_pair(it.first,make_pair("0", 0)));
			if(it.second->size > 4) globaldecl[it.first].second = it.second->size/4;
		}
	}
}

// write the specified symbol table into a csv file
void printSymbolTable(sym_table* table, string file_name){
	if(!dump_sym_table) return;
	FILE* file = fopen(file_name.c_str(), "w");
  	fprintf( file,"Name, Type, Size, isInitialized, Offset\n");
  	for(auto it: (*table)){
    	fprintf(file,"%s,%s,", it.first.c_str(), it.second->type.c_str());
		fprintf(file, "%d,%d,%d\n", (it.second)->size, (it.second)->init, (it.second)->offset);
  	}
  	fclose(file);
}

// return the total size of the user defined datatype
int getStructsize(string struct_name){
	struct_sym_table* temp = curr_struct_table;
	while(temp){
		if((*temp).find(struct_name)!=(*temp).end()){
			return (*temp)[struct_name].first;
		}
		temp = struct_parent_table[temp];
	}
	return 0;
}

// return the size of the given datatype
int getSize(string id){
  if(typeLookup(id)) return getStructsize(id);
  if(1) return 4;
  if(id == "char") return sizeof(char);
  if(id == "short") return sizeof(short);
  if(id == "short int") return sizeof(short int);
  if(id == "int") return sizeof(int);
  if(id == "bool") return sizeof(bool);
  if(id == "long int") return sizeof(long int);
  if(id == "long long") return sizeof(long long);
  if(id == "long long int") return sizeof(long long int);
  if(id == "float") return sizeof(float);
  if(id == "double") return sizeof(double);
  if(id == "long double") return sizeof(long double);
  if(id == "signed short int") return sizeof(signed short int);
  if(id == "signed int") return sizeof(signed int);
  if(id == "signed long int") return sizeof(signed long int);
  if(id == "signed long long") return sizeof(signed long long);
  if(id == "signed long long int") return sizeof(signed long long int);
  if(id == "unsigned short int") return sizeof(unsigned short int);
  if(id == "unsigned int") return sizeof(unsigned int);
  if(id == "unsigned long int") return sizeof(unsigned long int);
  if(id == "unsigned long long") return sizeof(unsigned long long);
  if(id == "unsigned long long int") return sizeof(unsigned long long int);
  return 4;
}