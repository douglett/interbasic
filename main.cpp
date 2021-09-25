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
	VAR_NULL=0,
	VAR_INTEGER,
	//VAR_FLOAT,
	VAR_STRING,
	VAR_ARRAY,
	//VAR_OBJECT,
};


struct Var {
	VAR_TYPE type;
	int32_t i;
	//_Float32 f;
	string s;
	vector<Var> arr;
	//map<string, Var> obj;
};


//struct TokList_ {
//	vector<string> tok;
//	int line, pos;
//
//	string& at(int i) {
//		if (i < 0 || i >= tok.size())
//			throw IBError("token out of range: " + to_string(i), line);
//		return tok[i];
//	}
//};
//typedef  vector<string>  TokList;


string join(const vector<string>& vs, const string& glue=", ") {
	string s;
	for (int i=0; i<vs.size(); i++)
		s += (i==0 ? "" : glue) + vs[i];
	return s;
}


struct InterBasic {
	struct frame { int lno; map<string, Var> vars; };
	vector<string> lines, tok;
	int lno = 0, pos = 0;
	map<string, Var> vars;
	vector<frame> callstack;


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

	vector<string> tokenize(const string& line) const {
		vector<string> vs;
		string s;
		for (int i=0; i<line.length(); i++) {
			char c = line[i];
			if      (isspace(c))  s = s.length() ? (vs.push_back(s), "") : "";
			else if (isalnum(c) || c == '_')  s += c;
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
		return vs;
	}


	// display
	void showlines() const {
		for (int i=0; i<lines.size(); i++)
			showtokens( tokenize(lines[i]), i );
	}
	void showtokens(const vector<string>& tok, int lno=0) const {
		printf("%02d  ::  ", lno+1);
		for (auto &t : tok)
			printf("[%s]  ", t.c_str());
		printf("\n");
	}
	void showenv() const {
		printf("env :: \n");
		for (auto& v : vars)
			printf("	%s : %s\n", v.first.c_str(), stringify(v.second).c_str() );
	}


	// runtime
	void runlines() {
		for (lno=0; lno<lines.size(); lno++)
			runline(lines[lno]);
	}
	void runline(const string& line) {
		tok = tokenize(line);
		//showtokens(tok, lno);
		auto cmd = tok.size() ? tok.at(0) : "";
		pos = 1;
		if      (cmd == "")         ;  // empty line
		else if (is_comment(cmd))   ;  // empty line (with comment)
		// dim / undim
		else if (cmd == "let" || cmd == "local") {
			if (cmd == "local" && callstack.size() == 0)  throw IBError("no local scope", lno);
			auto& id = tok_get();
			auto& eq = tok_get();
			auto& vv = cmd == "local" ? callstack.back().vars : vars;
			if (eq != "=")  throw IBError("missing =", lno);
			vv[id] = expr();
		}
		else if (cmd == "unlet" || cmd == "unlocal") {
			if (cmd == "unlocal" && callstack.size() == 0)  throw IBError("no local scope", lno);
			auto& id = tok.at(1);
			auto& vv = cmd == "unlocal" ? callstack.back().vars : vars;
			if (vv.count(id))  vv.erase(id);  // can unlet missing vars, for simplicity
		}
		// print command
		else if (cmd == "print") {
			string s;
			int ii = 0;
			for (int i=1; i<tok.size(); i++) {
				auto &t = tok.at(i);
				if      (t == ",")            printf(" ");   // space seperator
				else if (t == ";")            printf("\t");  // tab seperator
				else if (is_literal(t, &s))   printf("%s", s.c_str() );
				else if (is_integer(t, &ii))  printf("%d", ii );
				else if (is_identifier(t))    printf("%s", stringify(get_def(t)).c_str() );
				//else if (is_identifier(t))    printf("%s", stringify(expr()) );
				else    throw IBError("print error: ["+t+"]", lno);
			}
			printf("\n");
		}
		// ... various
		else if (cmd == "if") {
			auto v = expr();
			if (v.type != VAR_INTEGER)  throw IBError("expected integer");
			if (v.i == 0)               lno = find_end("if", lno+1);  // find matching end-if
		}
		else if (cmd == "call") {
			auto& id   = tok_get();
			int   argc = 0;
			callstack.push_back({ lno });  // new stack frame
			//callstack.back().vars["_ret"] = { VAR_NULL }:
			if (tok_peek() == ":") {  // args start
				tok_get();
				while (pos < tok.size())
					if      (is_comment(tok_peek()))  tok_get();
					else if (tok_peek() == ",")  tok_get();
					else    callstack.back().vars["_arg"+to_string(++argc)] = expr();
			}
			if    (sysfunc(id))  ;  // run system function, if possible
			else  lno = find_line({ "function", id }, 0);  // jump to function block definition
		}
		else if (cmd == "function")  lno = find_line({ "end", "function" }, lno);  // skip function block
		else if (cmd == "die")       lno = lines.size();  // jump to end, and so halt
		//else if (cmd == "return")    unstack();
		else if (cmd == "return") {
			if (callstack.size() == 0)  throw IBError("'return' outside of call");
			lno = callstack.back().lno,  callstack.pop_back();
		}
		else if (cmd == "end") {
			auto &block_type = tok.at(1);
			if      (block_type == "if")        ; // ignore for now
			//else if (block_type == "function")  unstack();
			else if (block_type == "function") {
				if (callstack.size() == 0)  throw IBError("'end function' outside of call");
				lno = callstack.back().lno,  callstack.pop_back();
			}
		}
		else    throw IBError("unknown command: " + cmd, lno);
	}


	// token parsing
	//int    tok_eol () const { return pos >= tok.size() || is_comment(tok[pos]); }
	const string& tok_peek(int ahead=0) const {
		static const string& blank = "";
		return pos+ahead >= tok.size() ? blank : tok[pos+ahead];
	}
	const string& tok_get () {
		if (pos >= tok.size())  throw IBError("expected token", lno);
		return tok[pos++];
	}


	// expression parsing
	Var expr() {
		Var  v = expr_atom();
		auto p = tok_peek();
		if (p=="=" || p=="!") {
			string cmp = tok_get() + (tok_peek() == "=" ? tok_get() : "");
			Var q = expr_atom(), r = { VAR_INTEGER };
			if (v.type != VAR_INTEGER || q.type != VAR_INTEGER)  goto _err;
			if      (cmp == "==")  r.i = v.i == q.i;
			else if (cmp == "!=")  r.i = v.i != q.i;
			else    goto _err;
			return r;
			_err:
			throw IBError("bad comparison", lno);
		}
		return v;
	}
	Var expr_atom() {
		//printf("here\n");
		if    (is_identifier(tok_peek()))  return get_def(tok_get());
		else  return to_var(tok_get());
	}


	// runtime helpers
	Var& get_def(const string& id) {
		if (callstack.size() && callstack.back().vars.count(id))  return callstack.back().vars[id];
		if (vars.count(id))  return vars.at(id);
		throw IBError("undefined variable: "+id);
	}
	int find_line(const vector<string>& pattern, int start) const {
		for (int i=start; i<lines.size(); i++) {
			auto tok = tokenize(lines[i]);
			if (tok.size() < pattern.size())  continue;
			for (int j=0; j<pattern.size(); j++)
				if (tok[j] != pattern[j])  goto _next;
			return i;
			_next:  ;
		}
		throw IBError("find_line: not found: " + join(pattern));
	}
	int find_end(const string& type, int start) const {
		int nest = 1;
		for (int i=start; i<lines.size(); i++) {
			auto tok = tokenize(lines[i]);
			if      (tok.size() < 2)  ;
			else if (tok.at(0) == type)  nest++;
			else if (tok.at(0) == "end" && tok.at(1) == type)  nest--;
			if (nest == 0)  return i;
		}
		throw IBError("find_end: no matching 'end " + type + "'", start);
	}

//	int unstack() {
//		if (callstack.size() == 0)  throw IBError("can't return from function", lno);
//		lno = callstack.back().lno;
//		//auto& parent_vars = callstack.size() > 1 ? callstack[callstack.size()-2].vars : vars;
//		//parent_vars["_ret"] = callstack.back().vars["_ret"];
//		callstack.pop_back();
//		return 0;
//	}

	// runtime system functions
	int sysfunc(const string& id) {
		// array or string length
		if (id == "len") {
			auto& v = get_def("_arg1");
			auto& r = vars["_ret"] = { VAR_INTEGER, 0 };
			if      (v.type == VAR_STRING)  r.i = v.s.length();
			else if (v.type == VAR_ARRAY)   r.i = v.arr.size();
			else    throw IBError("expected array or string", lno);
		}
		// array position
		else if (id == "at") {
			auto& v = get_def("_arg1");
			auto& p = get_def("_arg2");
			if (v.type != VAR_ARRAY || p.type != VAR_INTEGER)  throw IBError("expected array, integer", lno);
			if (p.i < 0 || p.i >= v.arr.size())  throw IBError("index out of range: "+to_string(p.i), lno);
			vars["_ret"] = v.arr.at(p.i);
		}
		// split string by whitespace
		else if (id == "split") {
			string s;
			auto& v = get_def("_arg1");
			auto& r = vars["_ret"] = { VAR_ARRAY };
			if (v.type != VAR_STRING)  throw IBError("expected string", lno);
			for (auto c : v.s)
				if (isspace(c)) {
					if (s.size()) r.arr.push_back({ VAR_STRING, .s=s });
					s = "";
				}
				else  s += c;
			if (s.size()) r.arr.push_back({ VAR_STRING, .s=s });
		}
		else  return 0;
		return 1;
	}



	// type conversion
	Var to_var(const string& s) const {
		Var v = { VAR_NULL };
		if      (s == "null")          return v;
		else if (is_integer(s, &v.i))  return v.type=VAR_INTEGER, v;
		else if (is_literal(s, &v.s))  return v.type=VAR_STRING, v;
		throw IBError("expected Var, got ["+s+"]", lno);
	}
	string stringify(const Var& v) const {
		switch (v.type) {
		case VAR_NULL:     return "null";
		case VAR_INTEGER:  return std::to_string(v.i);
		case VAR_STRING:   return v.s;
		case VAR_ARRAY:    return "array:"+to_string(v.arr.size());
		}
		throw IBError();
	}
	// type identification
	int is_integer(const string& s, int *r=NULL) const {
		if (s.length() == 0)  return 0;
		for (int i=0; i<s.length(); i++)
			if (!isdigit(s[i]))  return 0;
		if (r)  *r = stoi(s);
		return 1;
	}
	int is_literal(const string& s, string *r=NULL) const {
		if (s.length() < 2)  return 0;
		if (s.front() != '"' || s.back() != '"')  return 0;
		if (r)  *r = s.substr(1, s.length()-2);
		return 1;
	}
	int is_identifier(const string& s) const {
		if (s.length() == 0)  return 0;
		if (!isalpha(s[0]) && s[0] != '_')  return 0;
		for (int i=1; i<s.length(); i++)
			if (!isalnum(s[i]) && s[i] != '_')  return 0;
		return 1;
	}
	int is_comment(const string& s) const {
		return s.length() && s[0] == '#';
	}

};


int main() {
	printf("hello world\n");
	InterBasic bas;
	bas.load("test.bas");
	printf("-----\n");
	//bas.showlines();
	bas.runlines();
	printf("-----\n");
	bas.showenv();
}