/*
  This file is part of the C++ CmdParser utility.
  Copyright (c) 2015 - 2019 Florian Rappl
*/

#pragma once
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <functional>

namespace cli
{

	/// Class used to wrap integer types to specify desired numerical base for specific argument parsing
	template <typename T, int numericalBase = 0>
	class NumericalBase
	{
	public:

		/// This constructor required for correct AgrumentCountChecker initialization
		NumericalBase() : value(0), base(numericalBase)
		{}

		/// This constructor required for default value initialization
		/// \param val comes from default value
		NumericalBase(T val) : value(val), base(numericalBase)
		{}

		operator T () const
		{
			return this->value;
		}
		operator T * ()
		{
			return this->value;
		}

		T             value;
		unsigned  int base;
	};



	struct CallbackArgs
	{
		const std::vector<std::string>& arguments;
		std::ostream& output;
		std::ostream& error;
	};


	template<typename T>
	using ValidationFunction = std::function<bool(const T&, std::ostream&, std::ostream&)>;


	class Parser
	{
	private:
		class CmdBase
		{
		public:
			explicit CmdBase(const std::string& name, const std::string& alternative, const std::string& description, bool required, bool dominant, bool variadic)
				:	name(name),
					command(name.size() > 0 ? "-" + name : ""),
					alternative(alternative.size() > 0 ? "--" + alternative : ""),
					description(description),
					required(required),
					handled(false),
					arguments({}),
					dominant(dominant),
					variadic(variadic)
			{
			}

			virtual ~CmdBase()
			{
			}


			virtual std::string print_value() const = 0;
			virtual bool parse(std::ostream& output, std::ostream& error) = 0;
			virtual bool validate(std::ostream& output, std::ostream& error) = 0;
			virtual std::string	usage() const
			{
				std::stringstream ss;

				if(command.empty() && alternative.empty())
					ss << "\tDEFAULT" << std::endl;
				else
					ss << "\t" << command << ",\t" << alternative << std::endl;

				if (required == true)
				{
					ss << "\t\t(required)";
				}
				else
				{
					ss << "\t\tDefault:\t'" + print_value() << "'" << std::endl;
					ss << "\t\t[optional] ";
				}

				ss << description << std::endl << std::endl;

				return ss.str();
			}

			bool is(const std::string& given) const
			{
				return given == command || given == alternative;
			}

		public:		
			std::string 	name;
			std::string 	command;
			std::string 	alternative;
			std::string 	description;
			bool 			required;
			bool 			handled;
			bool const 		dominant;
			bool const 		variadic;			
			std::vector<std::string> arguments;
		};

		template<typename T>
		struct ArgumentCountChecker
		{
			static constexpr bool Variadic = false;
		};

		template<typename T>
		struct ArgumentCountChecker<cli::NumericalBase<T>>
		{
			static constexpr bool Variadic = false;
		};

		template<typename T>
		struct ArgumentCountChecker<std::vector<T>>
		{
			static constexpr bool Variadic = true;
		};

		template<typename T>
		class CmdFunction final : public CmdBase {
		public:
			explicit CmdFunction(const std::string& name, const std::string& alternative, const std::string& description, bool required, bool dominant)
				:	CmdBase(name, alternative, description, required, dominant, ArgumentCountChecker<T>::Variadic)
			{
			}

			virtual bool parse(std::ostream& output, std::ostream& error)
			{
				try
				{
					CallbackArgs args { arguments, output, error };
					value = callback(args);
					return true;
				}
				catch(const std::exception& e)
				{
					error << "ERROR: Failed parsing function's arguments: " << std::endl;
					
					for(const auto& a : arguments)
						error << a << ", " << std::endl;

					error << e.what() << std::endl;
					// error << Parser::print_help();

					return false;
				}
			}

			virtual bool validate(std::ostream& output, std::ostream& error) override
			{
				return true;
			}

			virtual std::string print_value() const
			{
				return "";
			}

			std::function<T(CallbackArgs&)> callback;
			T value;
		};

		template<typename T>
		class CmdArgument final : public CmdBase {
		public:
			explicit CmdArgument(const std::string& name, const std::string& alternative, const std::string& description, bool required, bool dominant, ValidationFunction<T> vf = nullptr)
				:	CmdBase(name, alternative, description, required, dominant, ArgumentCountChecker<T>::Variadic)
				,	valFun(vf)
			{
			}

			virtual bool parse(std::ostream& output, std::ostream& error)
			{
				T result;
				try
				{
					// Use default
					if(!required && arguments.empty())
						return true;

					value = Parser::parse(arguments, value);
					return true;
				}
				catch(const std::exception& e)
				{
					if(name.empty())
						error << "ERROR: Parsing 'default' command arguments: ";
					else
						error << "ERROR: Parsing '" << name << "' command arguments: ";

					if(arguments.empty())
					{
						error << "no arguments provided";
					}
					else
					{
						for(const auto& a : arguments)
							error << a << ", " << std::endl;
					}

					error << e.what() << std::endl;					

					return false;
				}
			}

			virtual bool validate(std::ostream& output, std::ostream& error) override
			{
				if(valFun != nullptr)
					return valFun(value, output, error);

				return true;
			}

			virtual std::string print_value() const
			{
				return stringify(value);
			}

			T value;
			ValidationFunction<T> valFun = nullptr;
		};



		static int parse(const std::vector<std::string>& elements, const int&, int numberBase = 0)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stoi(elements[0], 0, numberBase);
		}

		static bool parse(const std::vector<std::string>& elements, const bool& defval)
		{
			if (elements.size() != 0)
				throw std::runtime_error("A boolean command line parameter cannot have any arguments.");

			return !defval;
		}

		static double parse(const std::vector<std::string>& elements, const double&)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stod(elements[0]);
		}

		static float parse(const std::vector<std::string>& elements, const float&)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stof(elements[0]);
		}

		static long double parse(const std::vector<std::string>& elements, const long double&)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stold(elements[0]);
		}

		static unsigned int parse(const std::vector<std::string>& elements, const unsigned int&, int numberBase = 0)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return static_cast<unsigned int>(std::stoul(elements[0], 0, numberBase));
		}

		static unsigned long parse(const std::vector<std::string>& elements, const unsigned long&, int numberBase = 0)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stoul(elements[0], 0, numberBase);
		}

		static unsigned long long parse(const std::vector<std::string>& elements, const unsigned long long&, int numberBase = 0)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stoull(elements[0], 0, numberBase);
		}

		static long long parse(const std::vector<std::string>& elements, const long long&, int numberBase = 0)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stoll(elements[0], 0, numberBase);
		}

		static long parse(const std::vector<std::string>& elements, const long&, int numberBase = 0)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return std::stol(elements[0], 0, numberBase);
		}

		static std::string parse(const std::vector<std::string>& elements, const std::string&)
		{
			if (elements.size() != 1)
				throw std::bad_cast();

			return elements[0];
		}

		template<class T>
		static std::vector<T> parse(const std::vector<std::string>& elements, const std::vector<T>&)
		{
			const T defval = T();
			std::vector<T> values { };
			std::vector<std::string> buffer(1);

			for (const auto& element : elements)
			{
				buffer[0] = element;
				values.push_back(parse(buffer, defval));
			}

			return values;
		}

		template <typename T> static T parse(const std::vector<std::string>& elements, const NumericalBase<T>& wrapper)
		{
			return parse(elements, wrapper.value, 0);
		}

		/// Specialization for number wrapped into numerical base
		/// \tparam T base type of the argument
		/// \tparam base numerical base
		/// \param elements
		/// \param wrapper
		/// \return parsed number
		template <typename T, int base> static T parse(const std::vector<std::string>& elements, const NumericalBase<T, base>& wrapper)
		{
			return parse(elements, wrapper.value, wrapper.base);
		}

		template<class T>
		static std::string stringify(const T& value)
		{
			return std::to_string(value);
		}

		template<class T, int base>
		static std::string stringify(const NumericalBase<T, base>& wrapper)
		{
			return std::to_string(wrapper.value);
		}

		template<class T>
		static std::string stringify(const std::vector<T>& values)
		{
			std::stringstream ss { };
			ss << "[ ";

			for (const auto& value : values)
			{
				ss << stringify(value) << " ";
			}

			ss << "]";
			return ss.str();
		}

		static std::string stringify(const std::string& str)
		{
			return str;
		}

	public:
		explicit Parser(int argc, const char** argv)
		{
			init(argc, argv);
		}

		explicit Parser(int argc, char** argv)
		{
			init(argc, argv);
		}

		
		Parser(int argc, const char** argv, std::string generalProgramDescriptionForHelpText)
			:	_general_help_text(std::move(generalProgramDescriptionForHelpText))
		{
			init(argc, argv);
		}

		Parser(int argc, char** argv, std::string generalProgramDescriptionForHelpText)
			:	_general_help_text(std::move(generalProgramDescriptionForHelpText))
		{
			init(argc, argv);
		}

		
		Parser()
		{			
		}
		
		Parser(std::string generalProgramDescriptionForHelpText)
			:	_general_help_text(std::move(generalProgramDescriptionForHelpText))
		{			
		}
		
		
		~Parser()
		{
			for (size_t i = 0, n = _commands.size(); i < n; ++i)
			{
				delete _commands[i];
			}
		}
		
		
		void init(int argc, char** argv)
		{
			_appname = argv[0];
			
			for (int i = 1; i < argc; ++i)
			{
				_arguments.push_back(argv[i]);
			}
			enable_help();
		}
		
		void init(int argc, const char** argv)
		{
			_appname = argv[0];
			
			for (int i = 1; i < argc; ++i)
			{
				_arguments.push_back(argv[i]);
			}
			enable_help();
		}

		
		bool has_help() const
		{
			for (const auto& command : _commands)
			{
				if (command->name == "h" && command->alternative == "--help")
					return true;
			}

			return false;
		}

		void enable_help()
		{
			set_callback(
				"h",
				"help",
				std::function<bool(CallbackArgs&)>
				(
					[this](CallbackArgs& args)
					{
						args.output << this->usage();
						// #pragma warning(push)
						// #pragma warning(disable: 4702)
						// exit(0);
						return false;
						// #pragma warning(pop)
					}
				),
				"",
				true
			);
		}

		void disable_help()
		{
			for (auto command = _commands.begin(); command != _commands.end(); ++command)
			{
				if ((*command)->name == "h" && (*command)->alternative == "--help")
				{
					_commands.erase(command);
					delete *command;
					break;
				}
			}
		}

		template<typename T>
		void set_default(bool is_required, const std::string& description = "", T defaultValue = T(), ValidationFunction<T> vf = nullptr)
		{
			auto command = new CmdArgument<T> { "", "", description, is_required, false, vf };
			command->value = defaultValue;
			_commands.push_back(command);
		}

		template<typename T>
		void set_required(const std::string& name, const std::string& alternative, const std::string& description = "", ValidationFunction<T> vf = nullptr, bool dominant = false)
		{
			auto command = new CmdArgument<T> { name, alternative, description, true, dominant, vf };
			_commands.push_back(command);
		}

		template<typename T>
		void set_optional(const std::string& name, const std::string& alternative, T defaultValue, const std::string& description = "", ValidationFunction<T> vf = nullptr, bool dominant = false)
		{
			auto command = new CmdArgument<T> { name, alternative, description, false, dominant, vf };
			command->value = defaultValue;
			_commands.push_back(command);
		}

		template<typename T>
		void set_callback(const std::string& name, const std::string& alternative, std::function<T(CallbackArgs&)> callback, const std::string& description = "", bool dominant = false)
		{
			auto command = new CmdFunction<T> { name, alternative, description, false, dominant };
			command->callback = callback;
			_commands.push_back(command);
		}

		inline void run_and_exit_if_error()
		{
			if (run() == false)
			{
				exit(1);
			}
		}

		inline bool run()
		{
			return run(std::cout, std::cerr);
		}

		inline bool run(std::ostream& output)
		{
			return run(output, std::cerr);
		}

		bool doesArgumentExist(std::string name, std::string altName)
		{
			for (const auto& argument : _arguments)
			{
				
				if(argument == '-'+ name || argument == altName)
				{
					return true;
				}
			}

			return false;
		}

		inline bool doesHelpExist()
		{
			return doesArgumentExist("h", "--help");
		}

		bool run(std::ostream& output, std::ostream& error)
		{
			if (_arguments.size() > 0)
			{
				auto current = find_default();

				for (size_t i = 0, n = _arguments.size(); i < n; ++i)
				{
					const auto& currArg = _arguments[i];
					auto isarg = currArg.size() > 0 && currArg[0] == '-';
					auto associated = isarg ? find(currArg) : nullptr;

					if (associated != nullptr)
					{
						current = associated;
						associated->handled = true;
					}
					else if (current == nullptr)
					{
						error << invalid_parameter(currArg);
						// error << no_default();
						return false;
					}
					else
					{
						if(!current->variadic)
						{
							if(current->arguments.empty())
								current->arguments.push_back(currArg);
							else if(isarg)
							{
								error << invalid_parameter(currArg);
								return false;
							}
							else
							{
								if(is_default(current))
									error << "'Default' command can have only one parameter." << std::endl;
								else
									error << "Command '" << current->name << "[" << current->alternative << "]'" << " can have only one parameter." << std::endl;

								error  << "Given parameter '" << currArg << "' is invalid in this context!" << std::endl;
								output << print_help();

								return false;
							}

							// If the current command is not variadic, then no more arguments
							// should be added to it. In this case, switch back to the default
							// command.
							current = find_default();
						}
						else
							current->arguments.push_back(currArg);

						current->handled = true;
					}
				}
			}

			// First, parse dominant arguments since they succeed even if required
			// arguments are missing.
			for (auto command : _commands)
			{
				if (command->handled && command->dominant && ( !command->parse(output, error) || !command->validate(output, error)))
				{
					error << "ERROR: The parameter '" << command->name << "' has invalid arguments. Usage:\n";
					error << command->usage();
					return false;
				}
			}

			// Next, check for any missing arguments.
			for (auto command : _commands)
			{
				if (command->required && !command->handled)
				{
					error << "ERROR: The parameter '" << command->name << "' is required. Usage:\n";
					error << command->usage();
					return false;
				}
			}

			// Finally, parse all remaining arguments.
			for (auto command : _commands)
			{
				if (command->handled && !command->dominant && ( !command->parse(output, error) || !command->validate(output, error) ) )
				{
					error << "ERROR: The parameter '" << command->name << "' has invalid arguments. Usage:\n";
					error << command->usage();
					return false;
				}
			}

			return true;
		}

		template<typename T>
		T get(const std::string& name) const
		{
			for (const auto& command : _commands)
			{
				if (command->name == name)
				{
					auto cmd = dynamic_cast<CmdArgument<T>*>(command);

					if (cmd == nullptr)
					{
						throw std::runtime_error("Invalid usage of the parameter " + name + " detected.");
					}

					return cmd->value;
				}
			}

			throw std::runtime_error("The parameter " + name + " could not be found.");
		}

		template<typename T>
		T get_default()
		{
			return get<T>("");
		}

		template<typename T>
		T get_if(const std::string& name, std::function<T(T)> callback) const
		{
			auto value = get<T>(name);
			return callback(value);
		}

		int requirements() const
		{
			int count = 0;

			for (const auto& command : _commands)
			{
				if (command->required)
				{
					++count;
				}
			}

			return count;
		}

		int commands() const
		{
			return static_cast<int>(_commands.size());
		}

		inline const std::string& app_name() const
		{
			return _appname;
		}

	protected:
		CmdBase* find(const std::string& name)
		{
			for (auto command : _commands)
			{
				if (command->is(name))
				{
					return command;
				}
			}

			return nullptr;
		}

		CmdBase* find_default()
		{
			for (auto command : _commands)
			{
				if (command->name == "")
				{
					return command;
				}
			}

			return nullptr;
		}

		bool is_default(const CmdBase* cmd) const
		{
			return cmd->command.empty() && cmd->alternative.empty();
		}

		std::string usage() const
		{
			std::stringstream ss { };
			ss << _general_help_text << "\n\n";
			ss << "Available parameters:\n\n";

			for (const auto& command : _commands)
			{
				ss << command->usage();
			}

			return ss.str();
		}

		std::string print_help() const
		{
			if (has_help())
				return "For more help use --help or -h.\n";

			return "";
		}

		std::string invalid_parameter(const std::string& param) const
		{
			std::stringstream ss { };
			ss << "ERROR: Invalid parameter '" << param << "'\n";
			ss << print_help();

			return ss.str();
		}


		const std::string &get_general_help_text() const
		{
			return _general_help_text;
		}

		void set_general_help_text(const std::string &generalHelpText)
		{
			_general_help_text = generalHelpText;
		}
	private:
		std::string _appname;
		std::string _general_help_text;
		std::vector<std::string> _arguments;
		std::vector<CmdBase*> _commands;
	};
}
