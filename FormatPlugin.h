#pragma once

//#include "Schema.h"

namespace dtmdl
{

	struct SimpleOutputter;
	struct Database;
	struct ClassDefinition;
	struct EnumDefinition;
	struct StructDefinition;
	struct TypeDefinition;
	struct FieldDefinition;
	struct TypeReference;
	using TemplateArgument = variant<uint64_t, TypeReference>;

	struct FormatPlugin
	{
		virtual ~FormatPlugin() noexcept = default;
		virtual string FormatName() = 0;
		virtual string ExportFileName() = 0;
		virtual string Export(Database const&) = 0;
	};

	struct SimpleOutputter
	{
		SimpleOutputter(ostream& out) : OutStream(out) {}

		ostream& OutStream;
		size_t Indentation = 0;
		bool NewLine = true;

		void WriteIndent() { if (NewLine) { for (size_t i = 0; i < Indentation; ++i) OutStream << "\t"; NewLine = false; } }
		template <typename... ARGS>
		void Write(string_view fmt, ARGS&&... args) { WriteIndent(); OutStream << vformat(fmt, make_format_args(forward<ARGS>(args)...)); }
		template <typename... ARGS>
		void WriteLine(string_view fmt, ARGS&&... args) { Write(fmt, forward<ARGS>(args)...); Nl(); }
		template <typename... ARGS>
		void WriteStart(string_view fmt, ARGS&&... args) { WriteLine(fmt, forward<ARGS>(args)...); Indent(); }
		template <typename... ARGS>
		void WriteEnd(string_view fmt, ARGS&&... args) { Unindent(); WriteLine(fmt, forward<ARGS>(args)...); }

		void Nl() { OutStream << "\n"; NewLine = true; }

		void Indent() { Indentation++; }
		void Unindent() { if (Indentation == 0) throw "invalid unindent"; Indentation--; }
	};

	string MemberName(Database const& db, FieldDefinition const* def);

};