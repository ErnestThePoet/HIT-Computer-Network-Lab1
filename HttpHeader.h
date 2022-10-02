#pragma once

#include <qstring.h>
#include <qstringlist.h>

class HttpHeader
{
private:
	QString method_;
	QString url_;
	QString http_version_;
	QString host_;

public:
	HttpHeader(const QString& header)
	{
		auto lines = header.split("\r\n");
		auto first_line_parts = lines[0].split(' ');

		if (first_line_parts.size() < 2)
		{
			// 此方法会使得对此请求的服务被终止
			this->method_ = "INVALID";
			return;
		}

		this->method_ = first_line_parts[0];
		this->url_ = first_line_parts[1];
		this->http_version_ = first_line_parts[2];

		for (int i = 1; i < lines.count(); i++)
		{
			if (lines[i].startsWith("host: ", Qt::CaseInsensitive))
			{
				this->host_ = lines[i].remove("host: ", Qt::CaseInsensitive);
				break;
			}
		}
	}

	QString method() const
	{
		return this->method_;
	}

	QString url() const
	{
		return this->url_;
	}

	QString http_version() const
	{
		return this->http_version_;
	}

	QString host() const
	{
		return this->host_;
	}
};

