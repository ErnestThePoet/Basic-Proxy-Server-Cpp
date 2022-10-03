#pragma once

#include <string>
#include <vector>
#include <algorithm>

class HttpHeader
{
private:
	std::string method_;
	std::string url_;
	std::string host_;

public:
	HttpHeader(const std::vector<char>& raw)
	{
		const std::string kCrLf = "\r\n";
		const std::string kSpace = " ";
		const std::string kHost = "Host: ";

		auto first_line_end=std::search(raw.cbegin(), raw.cend(), 
			kCrLf.cbegin(), kCrLf.cend());
		auto method_end = std::search(raw.cbegin(), first_line_end, 
			kSpace.cbegin(), kSpace.cend());

		if (method_end == first_line_end)
		{
			this->method_ = "";
			return;
		}

		auto url_end = std::search(method_end + 1, first_line_end, 
			kSpace.cbegin(), kSpace.cend());

		this->method_ = std::string(raw.begin(), method_end);
		this->url_ = std::string(method_end + 1, url_end);

		auto current_line_start = first_line_end + 2;

		while (true)
		{
			auto current_line_end = 
				std::search(current_line_start, raw.cend(), 
					kCrLf.cbegin(), kCrLf.cend());

			auto host_start = std::search(current_line_start, current_line_end,
				kHost.cbegin(), kHost.cend());

			if (host_start == current_line_start)
			{
				this->host_ = std::string(host_start + kHost.size(), current_line_end);
				break;
			}

			if (current_line_end == raw.cend())
			{
				break;
			}

			current_line_start = current_line_end + 2;
		}
	}

	std::string method() const
	{
		return this->method_;
	}

	std::string url() const
	{
		return this->url_;
	}

	std::string host() const
	{
		return this->host_;
	}
};

