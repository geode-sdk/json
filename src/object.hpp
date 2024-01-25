class matjson::ObjectImpl {
public:
	std::vector<Object::value_type> data;
};

using matjson::ObjectImpl;

Object::Object() : m_impl(std::make_unique<ObjectImpl>()) {}
Object::Object(const Object& object) : m_impl(std::make_unique<ObjectImpl>(*object.m_impl.get())) {}
Object::Object(Object&& object) : m_impl(std::move(object.m_impl)) {}
Object::Object(std::initializer_list<value_type> init) : m_impl(std::make_unique<ObjectImpl>(ObjectImpl{init})) {}
Object::~Object() {}

Object& Object::operator=(Object other) {
	m_impl.swap(other.m_impl);
	return *this;
}

Object::iterator Object::begin() { return m_impl->data.begin(); }
Object::iterator Object::end() { return m_impl->data.end(); }
Object::const_iterator Object::begin() const { return m_impl->data.begin(); }
Object::const_iterator Object::end() const { return m_impl->data.end(); }
Object::const_iterator Object::cbegin() const { return m_impl->data.cbegin(); }
Object::const_iterator Object::cend() const { return m_impl->data.cend(); }

bool Object::operator==(const Object& other) const {
	for (const auto& [key, value] : *this) {
		if (auto it = other.find(key); it != other.end()) {
			if (value != it->second) return false;
		} else {
			return false;
		}
	}
	return true;
}

bool Object::operator<(const Object& other) const {
	return m_impl->data < other.m_impl->data;
}

bool Object::operator>(const Object& other) const {
	return m_impl->data > other.m_impl->data;
}

Object::iterator Object::find(std::string_view key) {
	auto end = this->end();
	for (auto it = this->begin(); it != end; ++it) {
		if (it->first == key) return it;
	}
	return end;
}

Object::const_iterator Object::find(std::string_view key) const {
	auto end = this->cend();
	for (auto it = this->cbegin(); it != end; ++it) {
		if (it->first == key) return it;
	}
	return end;
}

std::pair<Object::iterator, bool> Object::insert(const Object::value_type& value) {
	if (auto it = this->find(value.first); it != this->end()) {
		return {it, false};
	} else {
		m_impl->data.push_back(value);
		return {--m_impl->data.end(), true};
	}
}

size_t Object::erase(std::string_view key) {
	if (auto it = this->find(key); it != this->end()) {
		m_impl->data.erase(it);
		return 1;
	} else {
		return 0;
	}
}

Object::iterator Object::erase(const_iterator it) {
	return m_impl->data.erase(it);
}

size_t Object::size() const {
	return m_impl->data.size();
}

bool Object::empty() const {
	return m_impl->data.empty();
}

bool Object::contains(std::string_view key) const {
	return this->count(key);
}

size_t Object::count(std::string_view key) const {
	return this->find(key) == this->end() ? 0 : 1;
}

void Object::clear() {
	m_impl->data.clear();
}

Value& Object::operator[](std::string_view key) {
	if (auto it = this->find(key); it != this->end()) {
		return it->second;
	} else {
		m_impl->data.push_back({std::string(key), Value{}});
		return m_impl->data.back().second;
	}
}
