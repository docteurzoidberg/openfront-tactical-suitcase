#include <stddef.h>

// NOTE: This project intentionally embeds the server TLS certificate and private key
// as plain C strings to avoid PlatformIO/ESP-IDF build issues with generated .S
// sources in some environments.
//
// If you regenerate these, keep the trailing newline after END lines.

const char ots_server_cert_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIID4zCCAsugAwIBAgIUDglWwBNTAObUnVi2I4bV1l+bzxcwDQYJKoZIhvcNAQEL\n"
"BQAwajELMAkGA1UEBhMCVVMxDjAMBgNVBAgMBVN0YXRlMQ0wCwYDVQQHDARDaXR5\n"
"MRIwEAYDVQQKDAlPcGVuRnJvbnQxDDAKBgNVBAsMA09UUzEaMBgGA1UEAwwRb3Rz\n"
"LWZ3LW1haW4ubG9jYWwwHhcNMjUxMjIzMDU1NTM0WhcNMzUxMjIxMDU1NTM0WjBq\n"
"MQswCQYDVQQGEwJVUzEOMAwGA1UECAwFU3RhdGUxDTALBgNVBAcMBENpdHkxEjAQ\n"
"BgNVBAoMCU9wZW5Gcm9udDEMMAoGA1UECwwDT1RTMRowGAYDVQQDDBFvdHMtZnct\n"
"bWFpbi5sb2NhbDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKLibYjP\n"
"7RFiQoas/D7DhN5m9/Agp4o50Kh7dANZFpg6weLLzzZLqW5cN9C05H+qjCoiIBJx\n"
"1tmax3Zc/xNNhkLTBt/AccvNSH1uv+j7bGc6CXicef7fechGyQGs9D4epKNxqJiN\n"
"yVK+3wpI+EfQ+FSUpRbNxMzA/7RwNua1vWIC7WVuIAm+Wolc7DnwKBkIwmFknCu1\n"
"mg6fA0SKi8mDfUNTUQof1F0BfaPy1f2M7EHDNb9L8K6d+z8Vjn+gfDl5Rb8awLCY\n"
"1vshgfRDbkst43Pu4lWIxI678pyUXyHRWB5Wi4tAGoHC4x2x0KY84dRXszv1+71w\n"
"nhPzc/kz6YVsbCkCAwEAAaOBgDB+MB0GA1UdDgQWBBTGuF8EcP1bNH9AwT0edBtL\n"
"zdfCNDAfBgNVHSMEGDAWgBTGuF8EcP1bNH9AwT0edBtLzdfCNDAPBgNVHRMBAf8E\n"
"BTADAQH/MCsGA1UdEQQkMCKCEW90cy1mdy1tYWluLmxvY2FsggcqLmxvY2FshwTA\n"
"qAEBMA0GCSqGSIb3DQEBCwUAA4IBAQBLEb4QoeMyNSPy1RWtS7kONkk15rX672PZ\n"
"YfDau/InBRHzUfIpc1cmtWX213UERvNw1tueKY7IzqysOigZmfLzNPmiex2WzEA6\n"
"AyXBtcg4gaivqYbVcLtjw5aYi7txhaQMXU2fRwyL71EL8eR3xt67H6h35O2gNkwX\n"
"xDdXGFDcVQ8hl/0Y9uBcFQRknXtLBAUgxSBY76Kf8C+YYh/PaATMbdAnNtnbG70S\n"
"U9/qq1hv8Of5Oa66FylJdd7mUYcMskuTfxxEmexPHV5hR5pDdG+nsHhdI/Ku1nOy\n"
"L+FCwSHLyfGi8XDSKpPae1dGBmjAnu3eEI8sdtOX1Xoa+HaGYE7r\n"
"-----END CERTIFICATE-----\n";

// mbedTLS expects PEM buffers to be NUL-terminated; include the terminator.
const size_t ots_server_cert_pem_len = sizeof(ots_server_cert_pem);

const char ots_server_key_pem[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCi4m2Iz+0RYkKG\n"
"rPw+w4TeZvfwIKeKOdCoe3QDWRaYOsHiy882S6luXDfQtOR/qowqIiAScdbZmsd2\n"
"XP8TTYZC0wbfwHHLzUh9br/o+2xnOgl4nHn+33nIRskBrPQ+HqSjcaiYjclSvt8K\n"
"SPhH0PhUlKUWzcTMwP+0cDbmtb1iAu1lbiAJvlqJXOw58CgZCMJhZJwrtZoOnwNE\n"
"iovJg31DU1EKH9RdAX2j8tX9jOxBwzW/S/Cunfs/FY5/oHw5eUW/GsCwmNb7IYH0\n"
"Q25LLeNz7uJViMSOu/KclF8h0VgeVouLQBqBwuMdsdCmPOHUV7M79fu9cJ4T83P5\n"
"M+mFbGwpAgMBAAECggEAAcjpopnFqx64pCVl27G0hWPbNGdFMrVsuQXmSOmbXuZo\n"
"7LlnPzzvwbjytx+eT9xQbh9C2vTioe6JYD2HYYCkV5vcm8vN62nn0RELOwwzPUPU\n"
"af0L9Kwh91z4M6OfpwUasHxMuMQ3+CdFhiUzzESozfN1hiUR0p0MSD/BCEDmTvo9\n"
"AQ01+7DSdeiR3JBBvXV9xD6qeov2csBsrazbpNsGej115Q/D3prR3W0wOy35F1NH\n"
"IeCqYJudbynubpDD4MNKSWb6I9Zx06l8+fb3cYzu5X5Pf0pa6wQhxjtOG2JsYWFy\n"
"/PDRm228AFXRHBjkBP64/gHDG1hrTx21AxAp10/bMQKBgQDV+w//F7tC6T0hr9Fl\n"
"65r1HjdsHPDmWhDcsOUdiUv12YQA+0k1GPozFLn8BzfLU5B0O32L5lTFz6JQTkB/\n"
"uqsf0GAmjdoekhpHKUw379AXpV51i8OeE4uFd9/M4VS0UxzbUVei/OChdPWkJdO5\n"
"K4mp6jakzsMcr9g4sI7zijBhEQKBgQDC3rpEc1n50knZYA6yl+9kdgtJYaCQsYal\n"
"F8y1Vns7NZtlcGnUeQhTshgLEgVbpNqV9+nuJ2zi/PpFi+VrqeyniPKk5XG0rHg6\n"
"8Xx7G44eW+zkLJ0EIFtpq/Zx1VpHRFtg29IUmICa2CrtH2cF2pTqFvh4W+Lg3yHT\n"
"USMwuIvZmQKBgQC7SFJn4k1z8tAee9O6cMvfXeMELRBrjMjVX74oa9KgCxEOCuG4\n"
"J3RU4P8nJuoee7UjBi6ME7x+pYxa3SJ2qNq9rZN6Kw2NVDLLtcmP68Ul7QcPupQr\n"
"9s9WseSfXVjVuyi2jCs37DxvE/8q/DCjEQgcP3I0LQN4SC/m7iEv5vMRAQKBgHcJ\n"
"GN0xXVf2dbf8Ll434zsJxJE5upxIZQg6BokK39HmSBtp1Ku+lzCRDJDOnElD5WZQ\n"
"fyxEFwZ9I+ARub3PmckpJZdGtPN5myPeWXzV8zVmT8l92xgnL8/YBH26px/7iJod\n"
"DTIZig2MWIRqd0MjJ23rRDI5ZtiYVJBB8u4S/RKZAoGAZLwYS12+BiNIe8QEGqzg\n"
"2kIe27GgXnq1xUFjRGPEp4+kDifNHJ+qyUiWCtnTkCF11QvT7qO3QERfD8XsnF0Q\n"
"SO/w8z+uOKF/lkCXI2/81xt/XJSecw6e/hWVGN5dVCg41S7wbuVMPiBgKjUu04q3\n"
"1ee6jzuZwqvbZ2NnkavZFa8=\n"
"-----END PRIVATE KEY-----\n";

// mbedTLS expects PEM buffers to be NUL-terminated; include the terminator.
const size_t ots_server_key_pem_len = sizeof(ots_server_key_pem);
