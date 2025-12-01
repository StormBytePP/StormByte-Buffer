#include <StormByte/buffer/external_producer.hxx>

using namespace StormByte::Buffer;

ExternalProducer::ExternalProducer(const ExternalReader& reader) noexcept: Producer() {
	m_external_reader = reader;
	m_rw_thread = std::thread([this]() {
		bool has_error = false;
		do {
			auto expected_data = m_external_reader();
			if (!expected_data) {
				has_error = true;
				break;
			}
			else {
				if (expected_data->empty()) {
					break;
				}
				else {
					// Write data to buffer
					has_error = !this->Write(*expected_data);
				}
			}
		} while (this->IsWritable() && !has_error);
		if (has_error)
			this->SetError();
		else
			this->Close();
	});
}

ExternalProducer::~ExternalProducer() {
	if (m_rw_thread.joinable()) {
		m_rw_thread.join();
	}
}