/*
 * Copyright (C) DreamLab Onet.pl Sp. z o. o.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <v8.h>
#include <algorithm>
#include <node.h>
#include <map>
#include <list>
#include <string>
#include <cstring>
#include <vector>
#include <UriParser.hpp>

#define THROW_IF_NULL(var) if (var.afterLast == NULL) { \
        return v8::ThrowException(v8::Exception::SyntaxError(v8::String::New("Bad string given"))); \
    }

#define THROW_IF_EMPTY(var) if (var.empty()) { \
return v8::ThrowException(v8::Exception::SyntaxError(v8::String::New("Bad string given"))); \
}

#define ENCODED_BRACKETS "%5B%5D"
#define BRACKETS "[]"

enum parseOptions {
    kProtocol = 1,
    kAuth = 1 << 1,
    kHost = 1 << 2,
    kPort = 1 << 3,
    kQuery = 1 << 4,
    kFragment = 1 << 5,
    kPath = 1 << 6,
    kAll = kProtocol | kAuth | kHost | kPort | kQuery | kFragment | kPath
};

static v8::Persistent<v8::String> protocol_symbol = NODE_PSYMBOL("protocol");
static v8::Persistent<v8::String> auth_symbol = NODE_PSYMBOL("auth");
static v8::Persistent<v8::String> host_symbol = NODE_PSYMBOL("host");
static v8::Persistent<v8::String> port_symbol = NODE_PSYMBOL("port");
static v8::Persistent<v8::String> query_symbol = NODE_PSYMBOL("query");
static v8::Persistent<v8::String> query_arr_suffix = NODE_PSYMBOL("queryArraySuffix");
static v8::Persistent<v8::String> fragment_symbol = NODE_PSYMBOL("fragment");
static v8::Persistent<v8::String> path_symbol = NODE_PSYMBOL("path");
static v8::Persistent<v8::String> user_symbol = NODE_PSYMBOL("user");
static v8::Persistent<v8::String> password_symbol = NODE_PSYMBOL("password");

static v8::Handle<v8::Value> parse(const v8::Arguments& args){
    v8::HandleScope scope;

    parseOptions opts = kAll;

    if (args.Length() == 0 || !args[0]->IsString()) {
        return v8::ThrowException(v8::Exception::TypeError(v8::String::New("First argument has to be string")));
    }

    if (args[1]->IsNumber()) {
        opts = static_cast<parseOptions>(args[1]->Int32Value());
    }

    v8::String::Utf8Value v8Url(args[0]->ToString());

    std::string url(*v8Url);

    if (url.size() == 0) {
        return v8::ThrowException(v8::Exception::TypeError(v8::String::New("String mustn't be empty")));
    }

    v8::PropertyAttribute attrib = (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete);
    v8::Local<v8::Object> data = v8::Object::New();
    http::url uri = http::ParseHttpUrl(url);

    if (!uri.protocol.empty() && opts & kProtocol) {
        THROW_IF_EMPTY(uri.protocol)
        // +1 here because we need : after protocol
        data->Set(protocol_symbol, v8::String::New(uri.protocol.c_str()), attrib);
    }

    if (opts & kAuth) {
        if (!uri.user.empty() && !uri.password.empty()) {
            v8::Local<v8::Object> authData = v8::Object::New();
            authData->Set(user_symbol, v8::String::New(uri.user.c_str()), attrib);
            authData->Set(password_symbol, v8::String::New(uri.password.c_str()), attrib);

            data->Set(auth_symbol, authData, attrib);
        }
    }

    if (!uri.host.empty() && opts & kHost) {
        data->Set(host_symbol, v8::String::New(uri.host.c_str()), attrib);
    }

    if (!uri.port.empty() && opts & kPort) {
        data->Set(port_symbol, v8::String::New(uri.port.c_str()), attrib);
    }

    if (!uri.query.empty() && opts & kQuery) {
        std::map<std::string, std::list<const char *> > paramsMap;
        std::vector<std::string> paramsOrder;
        char *query = new char[uri.query.size() +1];
        std::copy(uri.query.begin(), uri.query.end(), query);
        query[uri.query.size()] = '\0';
        const char *amp = "&", *sum = "=";
        char *queryParamPairPtr, *queryParam, *queryParamKey, *queryParamValue, *queryParamPtr;
        bool empty = true;
        v8::Local<v8::Object> qsSuffix = v8::Object::New();

        queryParam = strtok_r(query, amp, &queryParamPairPtr);

        v8::Local<v8::Object> queryData = v8::Object::New();
        bool arrayBrackets = false;
        while (queryParam) {
            if (*queryParam != *sum) {
                size_t len;
                empty = false;
                queryParamKey = strtok_r(queryParam, sum, &queryParamPtr);
                len = strlen(queryParamKey);
                if (len > (sizeof(ENCODED_BRACKETS) - 1) &&
                        !strncmp(queryParamKey + len - (sizeof(ENCODED_BRACKETS) - 1),
                                 ENCODED_BRACKETS,
                                 sizeof(ENCODED_BRACKETS) - 1)) {
                    arrayBrackets = true;
                    queryParamKey[len - (sizeof(ENCODED_BRACKETS) - 1)] = '\0';
                    qsSuffix->Set(v8::String::New(queryParamKey), v8::String::New(ENCODED_BRACKETS));
                } else if (len > (sizeof(BRACKETS) - 1) &&
                        !strncmp(queryParamKey + len - (sizeof(BRACKETS) - 1),
                                 BRACKETS,
                                 sizeof(BRACKETS) - 1)) {
                    arrayBrackets = true;
                    queryParamKey[len - (sizeof(BRACKETS) - 1)] = '\0';
                    qsSuffix->Set(v8::String::New(queryParamKey), v8::String::New(BRACKETS));
                }

                queryParamValue = strtok_r(NULL, sum, &queryParamPtr);
                if (paramsMap.find(queryParamKey) == paramsMap.end()) {
                    paramsOrder.push_back(queryParamKey);
                }
                paramsMap[queryParamKey].push_back(queryParamValue ? queryParamValue : "");
            }
            queryParam = strtok_r(NULL, amp, &queryParamPairPtr);
        }


        for (std::vector<std::string>::iterator it=paramsOrder.begin(); it!=paramsOrder.end(); ++it) {
            v8::Local<v8::String> key = v8::String::New(it->c_str());
            std::list<const char *> vals = paramsMap[*it];
            int arrSize = vals.size();
            if (arrSize > 1 || qsSuffix->Has(key)) {
                v8::Local<v8::Array> arrVal = v8::Array::New(arrSize);

                int i = 0;
                for (std::list<const char *>::iterator it2 = vals.begin(); it2 != vals.end(); ++it2) {
                    arrVal->Set(i, v8::String::New(*it2));
                    i++;
                }
                queryData->Set(key, arrVal);
            } else {
                queryData->Set(key, v8::String::New((vals.front())));
            }
        }

        //no need for empty object if the query string is going to be wrong
        if (!empty) {
            data->Set(query_symbol, queryData, attrib);
            if (arrayBrackets) {
                data->Set(query_arr_suffix, qsSuffix, attrib);
            }
        }
        delete[] query;
    }

    if (!uri.fragment.empty() && opts & kFragment) {
        data->Set(fragment_symbol, v8::String::New(uri.fragment.c_str()), attrib);
    }

    if (!uri.path.empty() && opts & kPath) {
        data->Set(path_symbol, v8::String::New(uri.path.c_str()), attrib);
    } else {
        data->Set(path_symbol, v8::String::New("/"), attrib);
    }


    return scope.Close(data);
}

void init (v8::Handle<v8::Object> target){
    v8::HandleScope scope;

    NODE_SET_METHOD(target, "parse", parse);
    NODE_DEFINE_CONSTANT(target, kProtocol);
    NODE_DEFINE_CONSTANT(target, kAuth);
    NODE_DEFINE_CONSTANT(target, kHost);
    NODE_DEFINE_CONSTANT(target, kPort);
    NODE_DEFINE_CONSTANT(target, kQuery);
    NODE_DEFINE_CONSTANT(target, kFragment);
    NODE_DEFINE_CONSTANT(target, kPath);
    NODE_DEFINE_CONSTANT(target, kAll);
}

NODE_MODULE(uriparser, init)
