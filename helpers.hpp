#pragma once
#include <string>
#include <vector>

using namespace std;


enum VAR_TYPE {
	VAR_NULL=0,
	VAR_INTEGER,
	//VAR_FLOAT,
	VAR_STRING,
	VAR_ARRAY,
	VAR_OBJECT,
};

struct Var {
	VAR_TYPE type;
	int32_t i;
	//_Float32 f;
	string s;
	//vector<Var> arr;
	//map<string, Var> obj;

	static const Var VNULL;
	static const Var ZERO;
	static const Var ONE;
	// static const Var FZERO;
	// static const Var FONE;
	static const Var EMPTYSTR;
};

const Var Var::VNULL    = { VAR_NULL };
const Var Var::ZERO     = { VAR_INTEGER, 0 };
const Var Var::ONE      = { VAR_INTEGER, 1 };
const Var Var::EMPTYSTR = { VAR_STRING, 0, "" };



struct IBError : std::exception {
	string msg, error_string;
	int lno;
	IBError(const string& _msg="", int _lno=-1) : msg(_msg), lno(_lno) {
		error_string = (msg.length() ? msg : "InterBasic runtime exception")
			+ (lno >= 0 ? ", line " + to_string(lno+1) : "");
	}
	virtual const char* what() const noexcept {
		return error_string.c_str();
	}
};



// string manipulation
string join(const vector<string>& vs, const string& glue=", ") {
	string s;
	for (int i=0; i<vs.size(); i++)
		s += (i==0 ? "" : glue) + vs[i];
	return s;
}

// type identification
int is_integer(const string& s, int *r=NULL) {
	if (s.length() == 0)  return 0;
	for (int i=0; i<s.length(); i++)
		if (!isdigit(s[i]))  return 0;
	if (r)  *r = stoi(s);
	return 1;
}
int is_literal(const string& s, string *r=NULL) {
	if (s.length() < 2)  return 0;
	if (s.front() != '"' || s.back() != '"')  return 0;
	if (r)  *r = s.substr(1, s.length()-2);
	return 1;
}
int is_identifier(const string& s) {
	if (s.length() == 0)  return 0;
	if (!isalpha(s[0]) && s[0] != '_')  return 0;
	for (int i=1; i<s.length(); i++)
		if (!isalnum(s[i]) && s[i] != '_')  return 0;
	return 1;
}
int is_comment(const string& s) {
	return s.length() && s[0] == '#';
}