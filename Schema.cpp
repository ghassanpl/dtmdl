#include "pch.h"

#include "Schema.h"
#include "Validation.h"

namespace dtmdl
{
	RecordDefinition const* TypeDefinition::AsRecord() const noexcept { return IsRecord() ? static_cast<RecordDefinition const*>(this) : nullptr; }
	StructDefinition const* TypeDefinition::AsStruct() const noexcept { return IsStruct() ? static_cast<StructDefinition const*>(this) : nullptr; }
	ClassDefinition const* TypeDefinition::AsClass() const noexcept { return IsClass() ? static_cast<ClassDefinition const*>(this) : nullptr; }
	EnumDefinition const* TypeDefinition::AsEnum() const noexcept { return IsEnum() ? static_cast<EnumDefinition const*>(this) : nullptr; }

	bool IsCopyable(TypeReference const& ref)
	{
		if (!ref.Type)
			return true;

		if (!ref.Type->IsCopyable())
			return false;
		for (auto& c : ref.OnlyTypeReferenceArguments())
			if (!IsCopyable(c))
				return false;
		return true;
	}

	//string TypeDefinition::QualifiedName(string_view sep) const noexcept { return format("{}{}{}", mSchema.Namespace, sep, mName); }

	string to_string(TypeReference const& tr)
	{
		return tr.ToString();
	}

	string TypeReference::ToString() const
	{
		using std::to_string;
		if (!Type)
			return "[none]";

		string base = Type->Name();
		if (TemplateArguments.size())
		{
			base += '<';
			bool first = true;
			for (auto& arg : TemplateArguments)
			{
				if (!first)
					base += ", ";
				base += visit([](auto const& val) { return to_string(val); }, arg);
				first = false;
			}
			base += '>';
		}
		return base;
	}

	json ToJSON(TypeReference const& ref)
	{
		if (!ref.Type)
			return json{};
		json result = json::object();
		result["name"] = ref.Type->Name();
		if (ref.TemplateArguments.size())
		{
			auto& args = result["args"] = json::array();
			for (auto& arg : ref.TemplateArguments)
				visit([&args](auto const& val) { args.push_back(val); }, arg);
		}
		return result;
	}

	void FromJSON(TypeReference& ref, Schema const& schema, json const& value)
	{
		ref.TemplateArguments.clear();
		if (value.is_null())
		{
			ref.Type = {};
		}
		else
		{
			auto& type = value.get_ref<json::object_t const&>();
			ref.Type = schema.ResolveType(type.at("name"));
			if (!ref.Type)
				throw std::runtime_error(format("type '{}' not found", (string)type.at("name")));

			if (auto it = type.find("args"); it != type.end())
			{
				ref.TemplateArguments.clear();
				for (auto& arg : it->second.get_ref<json::array_t const&>())
				{
					if (arg.is_number())
						ref.TemplateArguments.emplace_back((uint64_t)arg);
					else
						ref.TemplateArguments.push_back(TypeFromJSON(schema, arg));
				}
			}
		}
	}

	/*
	TypeReference::TypeReference(Schema const& schema, json const& val)
	{
		FromJSON(*this, schema, val);
	}
	*/

	TypeReference::TypeReference(TypeDefinition const* value, vector<TemplateArgument> args)
		: Type(value)
		, TemplateArguments(move(args))
	{
		if (value && value->TemplateParameters().size() > TemplateArguments.size())
			throw runtime_error(format("invalid number of arguments for type '{}': expected {}, got {}", value->Name(), value->TemplateParameters().size(), TemplateArguments.size()));
	}

	TypeReference::TypeReference(TypeDefinition const* value) noexcept
		: Type(value)
		, TemplateArguments(value ? value->TemplateParameters().size() : 0)
	{
	}

	FieldDefinition const* RecordDefinition::Field(size_t index) const
	{
		if (index >= mFields.size())
			return nullptr;
		return mFields[index].get();
	}

	FieldDefinition const* RecordDefinition::OwnField(string_view name) const
	{
		for (auto& field : mFields)
		{
			if (field->Name == name)
				return field.get();
		}
		return nullptr;
	}

	FieldDefinition const* RecordDefinition::OwnOrBaseField(string_view name) const
	{
		auto field = OwnField(name);
		if (field)
			return field;
		if (mBaseType.Type)
			return mBaseType.Type->AsRecord()->OwnOrBaseField(name);
		return nullptr;
	}

	size_t RecordDefinition::FieldIndexOf(FieldDefinition const* field) const
	{
		for (size_t i = 0; i < mFields.size(); ++i)
			if (mFields[i].get() == field)
				return i;
		return -1;
	}

	set<string> RecordDefinition::OwnFieldNames() const
	{
		set<string> result;
		for (auto& f : mFields)
			result.insert(f->Name);
		return result;
	}

	set<string> RecordDefinition::AllFieldNames() const
	{
		set<string> result;
		TypeDefinition const* rec = this;
		while (rec)
		{
			for (auto& f : rec->AsRecord()->mFields)
				result.insert(f->Name);
			rec = rec->BaseType().Type;
		}
		return result;
	}

	vector<FieldDefinition const*> RecordDefinition::AllFieldsOrdered() const
	{
		vector<FieldDefinition const*> result;

		if (auto type = BaseType(); type && type->IsRecord())
			result = type->AsRecord()->AllFieldsOrdered();
		ranges::transform(mFields, back_inserter(result), [](auto const& f) { return f.get(); });

		return result;
	}

	json RecordDefinition::ToJSON() const
	{
		json result = TypeDefinition::ToJSON();
		auto& fields = result["fields"] = json::array();
		for (auto& field : mFields)
			fields.push_back(field->ToJSON());
		return result;
	}
	void RecordDefinition::FromJSON(json const& value)
	{
		TypeDefinition::FromJSON(value);
		mFields.clear();
		auto& fields = value.at("fields").get_ref<json::array_t const&>();
		for (auto& field : fields)
			mFields.push_back(make_unique<FieldDefinition>(this, field));
	}

	int64_t EnumeratorDefinition::ActualValue() const
	{
		int64_t current = 0;
		for (auto e : ParentEnum->Enumerators())
		{
			current = e->Value.value_or(current);
			if (e == this)
				return current;
			++current;
		}
		throw "what";
	}

	void EnumeratorDefinition::FromJSON(json const& value)
	{
		Name = value.at("name").get_ref<json::string_t const&>();
		json const& v = value.at("value");
		if (v.is_null())
			Value = nullopt;
		else
			Value = int64_t(v);
		DescriptiveName = value.at("descriptive").get_ref<json::string_t const&>();
		Attributes = get(value, "attributes");
	}

	json TypeDefinition::ToJSON() const
	{
		json result = json::object();
		result["name"] = mName;
		result["base"] = mBaseType;
		if (mTemplateParameters.size())
		{
			auto& params = result["params"] = json::array();
			for (auto& param : mTemplateParameters)
				params.push_back(param.ToJSON());
		}
		return result;
	}

	void TypeDefinition::FromJSON(json const& value)
	{
		mName = value.at("name").get_ref<json::string_t const&>();
		dtmdl::FromJSON(mBaseType, Schema(), value.at("base"));
		if (auto it = value.find("params"); it != value.end())
		{
			mTemplateParameters.clear();
			auto& params = it->get_ref<json::array_t const&>();
			for (auto& param : params)
				mTemplateParameters.emplace_back().FromJSON(param);
		}
	}

	void TemplateParameter::FromJSON(json const& value)
	{
		Name = value.at("name").get_ref<json::string_t const&>();
		Qualifier = magic_enum::enum_cast<TemplateParameterQualifier>(value.at("qualifier").get_ref<json::string_t const&>()).value();
		string s = value.at("flags");
		string_ops::split(s, ",", [this](string_view s, bool) { Flags.set(magic_enum::enum_cast<TemplateParameterFlags>(s).value()); });
	}

	Schema::Schema()
	{
		mVoid = AddNative("void", "::dtmdl::NativeTypes::Void", {}, {}, {}, ICON_VS_CIRCLE_SLASH);
		AddNative("f32", "float", {}, {}, { TemplateParameterQualifier::Floating, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("f64", "double", {}, {}, { TemplateParameterQualifier::Floating, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("i8", "int8_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("i16", "int16_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("i32", "int32_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("i64", "int64_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("u8", "uint8_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("u16", "uint16_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("u32", "uint32_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("u64", "uint64_t", {}, {}, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
		AddNative("bool", "bool", {}, {}, { TemplateParameterQualifier::NotClass }, ICON_VS_SYMBOL_BOOLEAN);
		AddNative("string", "::dtmdl::NativeTypes::String", {}, {}, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_STRING);
		AddNative("bytes", "::dtmdl::NativeTypes::Bytes", {}, {}, { TemplateParameterQualifier::NotClass }, ICON_VS_FILE_BINARY);
		AddNative("flags", "::dtmdl::NativeTypes::Flags", vector{
			TemplateParameter{ "ENUM", TemplateParameterQualifier::Enum }
			}, {}, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_CHECKLIST);
		AddNative("list", "::dtmdl::NativeTypes::List", vector{
			TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass, TemplateParameterFlags::CanBeIncomplete }
			}, { BuiltInFlags::Markable }, { TemplateParameterQualifier::NotClass }, ICON_VS_SYMBOL_ARRAY);
		AddNative("array", "::dtmdl::NativeTypes::Array", vector{
			TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass },
			TemplateParameter{ "SIZE", TemplateParameterQualifier::Size }
			}, { BuiltInFlags::Markable }, { TemplateParameterQualifier::NotClass }, ICON_VS_LIST_ORDERED);
		AddNative("ref", "::dtmdl::NativeTypes::Ref", vector{
			TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class, TemplateParameterFlags::CanBeIncomplete }
			}, { BuiltInFlags::Markable }, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Pointer, TemplateParameterQualifier::Scalar }, ICON_VS_REFERENCES);
		AddNative("own", "::dtmdl::NativeTypes::Own", vector{
			TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class, TemplateParameterFlags::CanBeIncomplete }
			}, {BuiltInFlags::Markable, BuiltInFlags::NonCopyable}, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Pointer, TemplateParameterQualifier::Scalar }, ICON_VS_REFERENCES);
		AddNative("variant", "::dtmdl::NativeTypes::Variant", vector{
			TemplateParameter{ "TYPES", TemplateParameterQualifier::NotClass, TemplateParameterFlags::Multiple }
			}, { BuiltInFlags::Markable }, { TemplateParameterQualifier::NotClass }, ICON_VS_TASKLIST);

		AddNative("map", "::dtmdl::NativeTypes::Map", vector{
			TemplateParameter{ "KEY_TYPE", TemplateParameterQualifier::Scalar },
			TemplateParameter{ "VALUE_TYPE", TemplateParameterQualifier::NotClass, TemplateParameterFlags::CanBeIncomplete }
			}, { BuiltInFlags::Markable }, { TemplateParameterQualifier::NotClass }, ICON_VS_SYMBOL_OBJECT);

		AddNative("json", "::dtmdl::NativeTypes::JSON", {}, {}, { TemplateParameterQualifier::NotClass }, ICON_VS_JSON);

		string_view prefixes[] = { "b", "i", "u", "d", "" };
		string_view types[] = { "bool", "int", "unsigned", "double", "float" };
		for (int i = 0; i < 5; ++i)
		{
			for (int d = 2; d <= 4; ++d)
			{
				string name = format("{}vec{}", prefixes[i], d);
				string real_name = format("tvec<{}, {}>", types[i], d);

				AddNative(name, real_name, {}, {}, { TemplateParameterQualifier::NotClass }, ICON_VS_LOCATION);
			}
		}

		/// TODO: Color ? Or should we leave that to attributes?
		/// TODO: Path ?
		/// TODO: row<StructType> -> holds a variant<pair<StructTypeTable*, StructType*>, pair<string table_id, string row_key>> and is resolved when (de)serializing via a Database
		///				or can be resolved manually
	}

	TypeDefinition const* Schema::ResolveType(string_view name) const
	{
		for (auto& def : mDefinitions)
			if (def->Name() == name)
				return def.get();
		return nullptr;
	}

	TypeDefinition* Schema::ResolveType(string_view name)
	{
		for (auto& def : mDefinitions)
			if (def->Name() == name)
				return def.get();
		return nullptr;
	}

	EnumeratorDefinition const* EnumDefinition::Enumerator(size_t index) const
	{
		if (index >= mEnumerators.size())
			return nullptr;
		return mEnumerators[index].get();
	}

	EnumeratorDefinition const* EnumDefinition::Enumerator(string_view name) const
	{
		for (auto& e : mEnumerators)
		{
			if (e->Name == name)
				return e.get();
		}
		return nullptr;
	}

	size_t EnumDefinition::EnumeratorIndexOf(EnumeratorDefinition const* field) const
	{
		for (size_t i = 0; i < mEnumerators.size(); ++i)
			if (mEnumerators[i].get() == field)
				return i;
		return -1;
	}

	json EnumDefinition::ToJSON() const
	{
		json result = TypeDefinition::ToJSON();
		auto& fields = result["enumerators"] = json::array();
		for (auto& enumerator : mEnumerators)
			fields.push_back(enumerator->ToJSON());
		return result;
	}

	void EnumDefinition::FromJSON(json const& value)
	{
		TypeDefinition::FromJSON(value);
		mEnumerators.clear();
		auto& enumerators = value.at("enumerators").get_ref<json::array_t const&>();
		for (auto& enumerator : enumerators)
			mEnumerators.push_back(make_unique<EnumeratorDefinition>(this, enumerator));
	}

	BuiltinDefinition const* Schema::AddNative(string name, string native_name, vector<TemplateParameter> params, enum_flags<BuiltInFlags> flags, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers, string icon)
	{
		return AddType<BuiltinDefinition>(move(name), move(native_name), move(params), flags, applicable_qualifiers, move(icon));
	}

	bool Schema::IsParent(TypeDefinition const* parent, TypeDefinition const* potential_child)
	{
		if ((parent && !parent->IsRecord()) || (potential_child && !potential_child->IsRecord()))
			return false;

		while (potential_child)
		{
			if (parent == potential_child)
				return true;
			potential_child = potential_child->mBaseType.Type;
		}

		return false;
	}

	void CalculateDependencies(TypeReference const& ref, set<TypeDefinition const*>& dependencies)
	{
		if (!ref.Type)
			return;

		dependencies.insert(ref.Type);

		for (size_t i = 0; i < ref.Type->TemplateParameters().size(); ++i)
		{
			auto& param = ref.Type->TemplateParameters()[i];
			auto& arg = ref.TemplateArguments[i];

			if (param.MustBeComplete())
			{
				if (auto ref = get_if<TypeReference>(&arg))
					CalculateDependencies(*ref, dependencies);
			}
		}
	}

	template <typename T, typename E>
	enum_flags<E> FilterBy(T const* self, enum_flags<E> value)
	{
		enum_flags<E> result{};
		value.for_each([self, &result](E v) {
			if (IsFlagAvailable(self, v))
				result.set(v);
			});
		return result;
	}

	json FieldDefinition::ToJSON() const { return json::object({ { "name", Name },{ "type", dtmdl::ToJSON(FieldType) },{ "attributes", Attributes },{ "flags", FilterBy(this, Flags) } }); }

	void FieldDefinition::FromJSON(json const& value)
	{
		Name = value.at("name").get_ref<json::string_t const&>();
		dtmdl::FromJSON(FieldType, ParentRecord->Schema(), value.at("type"));
		Attributes = get(value, "attributes");
		Flags = get_array(value, "flags");
	}

	json ClassDefinition::ToJSON() const
	{
		auto result = RecordDefinition::ToJSON();
		result["flags"] = FilterBy(this, Flags);
		return result;
	}

	void ClassDefinition::FromJSON(json const& value)
	{
		RecordDefinition::FromJSON(value);
		Flags = get_array(value, "flags");
	}

	json StructDefinition::ToJSON() const
	{
		auto result = RecordDefinition::ToJSON();
		result["flags"] = FilterBy(this, Flags);
		return result;
	}

	void StructDefinition::FromJSON(json const& value)
	{
		RecordDefinition::FromJSON(value);
		Flags = get_array(value, "flags");
	}

	bool IsFlagAvailable(FieldDefinition const* fld, FieldFlags flag)
	{
		switch (flag)
		{
		case FieldFlags::Private: return fld->ParentRecord->IsClass();
		case FieldFlags::Transient: return fld->ParentRecord->IsClass();
		case FieldFlags::Getter: return fld->ParentRecord->IsClass();
		case FieldFlags::Setter: return fld->ParentRecord->IsClass();

		case FieldFlags::Unique:
		case FieldFlags::Indexed: return fld->ParentRecord->IsStruct()
			&& fld->ParentRecord->AsStruct()->Flags.contain(StructFlags::CreateTableType)
			&& ValidateTypeDefinition(fld->FieldType.Type, TemplateParameterQualifier::Scalar);
		}
		return true;
	}

	bool IsFlagAvailable(StructDefinition const* strukt, StructFlags flag)
	{
		return true;
	}

	bool IsFlagAvailable(ClassDefinition const* klass, ClassFlags flag)
	{
		switch (flag)
		{
		case ClassFlags::Abstract: return !klass->Flags.contain(ClassFlags::Final);
		case ClassFlags::Final: return !klass->Flags.contain(ClassFlags::Abstract);
		}
		return true;
	}

}