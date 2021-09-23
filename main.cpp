#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <map>

using namespace std;


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


enum VAR_TYPE {
	VT_NULL=0,
	VT_INTEGER,
	//VT_FLOAT,
	VT_STRING,
	//VT_ARRAY,
	//VT_OBJECT,
};


struct Var {
	VAR_TYPE vtype;
	int32_t i;
	//_Float32 f;
	string s;
	//vector<Var> arr;
	//map<string, Var> obj;
};


struct TokList {
	vector<string> list;
	int lno, pos;

	string& at(int i) {
		if (i < 0 || i >= list.size())
			throw IBError("token out of range: " + to_string(i), lno);
		return list[i];
	}
	int size() {
		return list.size();
	}
};
//typedef  vector<string>  TokList;


string join(const vector<string>& vs, const string& glue=", ") {
	string s;
	for (int i=0; i<vs.size(); i++)
		s += (i==0 ? "" : glue) + vs[i];
	return s;
}


struct InterBasic {
	vector<string> lines;
	int lno = 0;
	map<string, Var> vars;
	vector<int> callstack;

	int load(const string& fname) {
		fstream fs(fname, ios::in);
		if (!fs.is_open())
			return fprintf(stderr, "error loading file %s\n", fname.c_str()), 1;
		string s;
		while (getline(fs, s))
			lines.push_back(s);
		printf("loaded file: %s (%d)\n", fname.c_str(), (int)lines.size());
		return 0;
	}

	TokList tokenize(const string& line, int lno) const {
		TokList tok = { {}, lno, 0 };
		//vector<string> vs;
		auto &vs = tok.list;
		string s;
		for (int i=0; i<line.length(); i++) {
			char c = line[i];
			if      (isspace(c))  s = s.length() ? (vs.push_back(s), "") : "";
			else if (isalnum(c))  s += c;
			else if (c == '"') {
				s = s.length() ? (vs.push_back(s), "") : "";
				s += c;
				for (++i; i<line.length(); i++) {
					s += line[i];
					if (line[i] == '"')  break;
				}
			}
			else if (c == '#') {
				s = s.length() ? (vs.push_back(s), "") : "";
				vs.push_back(line.substr(i));
				break;
			}
			else {
				s = s.length() ? (vs.push_back(s), "") : "";
				vs.push_back(string() + c);
			}
		}
		if (s.length())  vs.push_back(s);
		return tok;
	}


	// display
	void showlines() const {
		for (int i=0; i<lines.size(); i++)
			showtokens( tokenize(lines[i], i) );
	}
	void showtokens(const TokList& tok) const {
		printf("%02d  ::  ", tok.lno+1);
		for (auto &t : tok.list)
			printf("[%s]  ", t.c_str());
		printf("\n");
	}


	// runtime
	void runlines() {
		for (lno=0; lno<lines.size(); lno++)
			runline(lines[lno]);
	}
	void runline(const string& line) {
		auto tok = tokenize(line, lno);
		//showtokens(tok, lno);
		auto cmd = tok.size() ? tok.at(0) : "";
		if      (cmd == "")         ;  // empty line
		else if (cmd.at(0) == '#')  ;  // empty line (with comment)
		else if (cmd == "let") {
			auto &id = tok.at(1);
			auto &eq = tok.at(2);
			auto &v  = tok.at(3);
			if (eq != "=")  throw IBError();
			vars[id] = to_var(v);
		}
		else if (cmd == "unlet") {
			auto& id = tok.at(1);
			if (vars.count(id))  vars.erase(id);  // can unlet missing vars, for simplicity
		}
		else if (cmd == "print") {
			string s;
			int ii = 0;
			for (int i=1; i<tok.size(); i++) {
				auto &t = tok.at(i);
				if      (t == ",")            printf(" ");   // space seperator
				else if (t == ";")            printf("\t");  // tab seperator
				else if (is_literal(t, &s))   printf("%s", s.c_str() );
				else if (is_identifier(t))    printf("%s", stringify(get_def(t)).c_str() );
				else if (is_integer(t, &ii))  printf("%d", ii );
				else    throw IBError();
			}
			printf("\n");
		}
		else if (cmd == "if") {
			auto &id = tok.at(1);
			auto &v  = get_def(id);
			if (v.vtype != VT_INTEGER)  throw IBError("expected integer");
			if (v.i == 0)               lno = find_end("if", lno+1);  // find matching end-if
		}
		else if (cmd == "call") {
			auto &id = tok.at(1);
			callstack.push_back(lno);
			lno = find_line({ "function", id }, 0);  // jump to function block definition
		}
		else if (cmd == "function")  lno = find_line({ "end", "function" }, lno);  // skip function block
		else if (cmd == "die")       lno = lines.size();  // jump to end, and so halt
		else if (cmd == "return") {
			if (callstack.size() == 0)  throw IBError("'return' outside of call");
			lno = callstack.back(), callstack.pop_back();
		}
		else if (cmd == "end") {
			auto &block_type = tok.at(1);
			if      (block_type == "if")        ; // ignore for now
			else if (block_type == "function") {
				if (callstack.size() == 0)  throw IBError("'end function' outside of call");
				lno = callstack.back(), callstack.pop_back();
			}
		}
		else    throw IBError("unknown command: " + tok.at(0), lno);
	}


	// runtime helpers
	Var& get_def(const string& id) {
		if (!vars.count(id))  throw IBError("undefined variable: "+id);
		return vars.at(id);
	}
	int find_line(const vector<string>& pattern, int start) const {
		for (int i=start; i<lines.size(); i++) {
			auto tok = tokenize(lines[i], i);
			if (tok.size() < pattern.size())  continue;
			for (int j=0; j<pattern.size(); j++)
				if (tok.list[j] != pattern[j])  goto _next;
			return i;
			_next:  ;
		}
		throw IBError("find_line: not found: " + join(pattern));
	}
	int find_end(const string& type, int start) const {
		int nest = 1;
		for (int i=start; i<lines.size(); i++) {
			auto tok = tokenize(lines[i], i);
			if      (tok.size() < 2)  ;
			else if (tok.at(0) == type)  nest++;
			else if (tok.at(0) == "end" && tok.at(1) == type)  nest--;
			if (nest == 0)  return i;
		}
		throw IBError("find_end: no matching 'end " + type + "'", start);
	}


	// type conversion
	Var to_var(const string& s) {
		Var v = { VT_NULL };
		if      (s == "null")          return v;
		else if (is_integer(s, &v.i))  return v.vtype=VT_INTEGER, v;
		else if (is_literal(s, &v.s))  return v.vtype=VT_STRING, v;
		throw IBError();
	}
	string stringify(const Var& v) {
		switch (v.vtype) {
		case VT_NULL:     return "null";
		case VT_INTEGER:  return std::to_string(v.i);
		case VT_STRING:   return v.s;
		}
		throw IBError();
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
};


int main() {
	printf("hello world\n");
	InterBasic bas;
	bas.load("test.bas");
	//bas.showlines();
	bas.runlines();
}