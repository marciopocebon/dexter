//DEXTER - Data EXfiltration TestER
//This file is part of DEXTER Project

//Written by : @maldevel
//Website : https ://www.twelvesec.com/
//GIT : https://github.com/twelvesec/dexter

//TwelveSec(@Twelvesec)

//This program is free software : you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//GNU General Public License for more details.

//You should have received a copy of the GNU General Public License
//along with this program.If not, see < http://www.gnu.org/licenses/>.

//For more see the file 'LICENSE' for copying permission.

#include "libreporter.h"
#include "libhttp.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "helper.h"
#include "libcrypt.h"
#include "libencode.h"
#include "libcurl.h"
#include "libmime.h"

#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

static std::wstring pick_random_useragent(std::set<std::wstring> uagents, std::wstring Protocol) {

	std::wstring useragent = helper::pick_random_useragent_fromfile(uagents);
	std::wcout << L"[" << Protocol << L"] " << L"User-Agent: " << useragent << std::endl;

	return useragent;
}

static void handle_data(std::string data, std::string password, std::wstring Protocol) {
	//char **details = { 0 };
	//int splitted = 0;
	std::string decrypted_data = libcrypt::decrypt(password, data);
	std::vector<std::string> details;

	std::wcout << "[" << Protocol << "] " << "Received " << Protocol << " packet. Details: ";

	//if ((splitted = helper::split_string(decrypted_data.c_str(), (DWORD)decrypted_data.length(), "&", &details)) != -1) {
	details = helper::split_string(decrypted_data.c_str(), '\n');
	//if ((splitted = helper::split_string(decrypted_data.c_str(), (DWORD)decrypted_data.length(), "&", &details)) != -1) {
	for (int i = 0; i < details.size(); i++) {
		if (i == details.size() - 1) {
			std::cout << details[i];
		}
		else {
			std::cout << details[i] << ", ";
		}
	}
	std::cout << std::endl;
}

void libreporter::test_http_protocol(std::wstring host, WORD port, std::wstring token_uri_method, std::wstring clients_uri_method, std::wstring tokenuri,
	std::wstring clients_uri, std::set<std::wstring> uagents, WORD clientid, std::string secret, std::string username,
	std::string password, std::string aespassword, std::string PoC_KEYWORD, bool IGNORE_CERT_UNKNOWN_CA, bool IGNORE_CERT_DATE_INVALID,
	bool HTTPS_CONNECTION) {

	char *downloaded = 0;
	HINTERNET internet = NULL, connection = NULL, request = NULL;
	const WCHAR *token_headers = L"Accept: application/json\r\nContent-Type: application/x-www-form-urlencoded\r\nConnection: close\r\n";
	bool result = false;

	std::wstring protocol = (HTTPS_CONNECTION ? L"HTTPS" : L"HTTP");

	std::wstring useragent = helper::pick_random_useragent_fromfile(uagents);
	std::wcout << L"[" << protocol << L"] " << L"User-Agent: " << useragent << std::endl;

	std::string token_data = "grant_type=password&client_id=" + std::to_string(clientid) + "&client_secret=" +
		secret + "&username=" + username + "&password=" + password + "&scope=*";

	std::wcout << L"[" << protocol << L"] " << L"Connecting to " << protocol << L" server" << std::endl;

	internet = libhttp::open(useragent);

	if (internet != NULL) {
		connection = libhttp::connect(internet, host, port);
	}

	if (!HTTPS_CONNECTION) {
		std::wcout << "[HTTP] " << "Warning! Transmitting unencrypted data over HTTP" << std::endl;
	}

	std::wcout << L"[" << protocol << L"] " << L"Requesting API token with " << protocol << L" packet" << std::endl;

	if (connection != NULL) {
		request = libhttp::json_request(connection, token_uri_method, tokenuri, (char*)token_data.c_str(), token_headers, IGNORE_CERT_UNKNOWN_CA,
			IGNORE_CERT_DATE_INVALID, HTTPS_CONNECTION);
	}

	if (request != NULL) {
		result = libhttp::retrieve_data(request, &downloaded);
	}

	if (result && downloaded != NULL) {

		rapidjson::Document token_response;
		token_response.Parse(downloaded);

		HeapFree(GetProcessHeap(), 0, downloaded);
		downloaded = NULL;

		std::wstring access_token = helper::read_string_value(&token_response, "access_token");
		std::wstring clients_headers = L"Accept: application/json\r\nContent-Type: application/x-www-form-urlencoded\r\nAuthorization: Bearer " +
			access_token + L"\r\nConnection: close\r\n";

		std::wcout << L"[" << protocol << L"] " << L"Sending data with " << protocol << L" packet" << std::endl;

		if (connection != NULL) {
			request = libhttp::json_request(connection, clients_uri_method, clients_uri, NULL,
				clients_headers.c_str(), IGNORE_CERT_UNKNOWN_CA, IGNORE_CERT_DATE_INVALID, HTTPS_CONNECTION);
		}

		if (request != NULL) {
			result = libhttp::retrieve_data(request, &downloaded);
		}

		if (result && downloaded != NULL) {
			rapidjson::Document clients_response;
			clients_response.Parse(downloaded);

			if (helper::read_bool_value(&clients_response, "success") == true) {

				std::wcout << L"[" << protocol << L"] " << L"Transmission succeeded" << std::endl;

				if (clients_response.HasMember("data") && clients_response["data"].IsArray()) {
					const rapidjson::Value& a = clients_response["data"].GetArray();
					for (rapidjson::SizeType i = 0; i < a.Size(); i++) {
						if (a[i].IsObject()) {
							for (rapidjson::Value::ConstMemberIterator itr = a[i].MemberBegin(); itr != a[i].MemberEnd(); ++itr) {
								if (itr->name != NULL) {
									std::string val(itr->name.GetString());
									if (val == "data" && itr->value != NULL) {
										handle_data(itr->value.GetString(), aespassword, protocol);
									}
								}
							}
						}
					}
				}

			}
			else {

				std::wcout << "[" << protocol << "] " << "Transmission failed" << std::endl;
			}
		}

		if (downloaded) {
			HeapFree(GetProcessHeap(), 0, downloaded);
			downloaded = NULL;
		}
	}

	if (request) {
		InternetCloseHandle(request);
		request = NULL;
	}

	if (connection) {
		InternetCloseHandle(connection);
		connection = NULL;
	}

	if (internet) {
		InternetCloseHandle(internet);
		internet = NULL;
	}
}

void libreporter::test_gmail_protocol(std::string gmail_imap, std::string gmail_imap_inbox_obj, std::string gmail_username, std::string gmail_password, std::string gmail_name,
	std::set<std::wstring> uagents, std::string aespassword, std::string PoC_KEYWORD) {

	int total = 0;
	int i = 0;
	MimeMessage *m = 0;
	std::vector<int> ids;

	libcurl::init();

	std::wstring useragent = pick_random_useragent(uagents, L"GMAIL");

	std::wcout << "[GMAIL] " << "Connecting to GMAIL SMTP server" << std::endl;

	std::wcout << "[GMAIL] " << "Sending data with GMAIL packet" << std::endl;

	std::string uagent(useragent.begin(), useragent.end());

	std::wcout << "[GMAIL] " << "Retrieving emails IDs" << std::endl;

	ids = libcurl::get_emails_ids(gmail_username, gmail_password, gmail_imap, "UID SEARCH (SUBJECT '" + PoC_KEYWORD + "')", uagent);

	std::wcout << "[GMAIL] " << "Retrieving emails" << std::endl;

	for (i = 0; i < ids.size(); i++) {
		if (libcurl::receive_email(&m, ids[i], gmail_imap_inbox_obj, gmail_username, gmail_password, uagent)) {

		}
	}

	libcurl::finalize();
}

