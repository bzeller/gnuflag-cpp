#include "gnuflag.h"

#include <getopt.h>
#include <map>
#include <exception>
#include <utility>
#include <string.h>

namespace GnuFlag
{

namespace {

  void appendToOptString ( const CommandOption &option, std::string &optstring )
  {
    if ( option.shortName ) {
      optstring += option.shortName;

      switch ( option.flags & CommandOption::ArgumentTypeMask ) {
        case CommandOption::RequiredArgument:
          optstring += ":";
          break;
        case CommandOption::OptionalArgument:
          optstring += "::";
          break;
        case CommandOption::NoArgument:
          break;
      }
    }
  }

  void appendToLongOptions ( const CommandOption &opt, std::vector<struct option> &opts )
  {
    int has_arg;
    switch ( opt.flags & CommandOption::ArgumentTypeMask ) {
      case CommandOption::NoArgument:
        has_arg = no_argument;
        break;
      case CommandOption::RequiredArgument:
        has_arg = required_argument;
        break;
      case CommandOption::OptionalArgument:
        has_arg = optional_argument;
        break;
    }

    //we do not use the flag and val types, instead we use optind to figure out what happend
    using OptType = struct option;
    opts.push_back(OptType{opt.name, has_arg, 0 ,0});
  }
}

/**
 * @class Value
 * Composite type to provide a generic way to write variables and get the default value for them.
 * This type should be only used directly when implementing a new argument type.
 */

/**
 * \param defValue takes a functor that returns the default value for the option as string
 * \param setter takes a functor that writes a target variable based on the argument input
 * \param argHint Gives a indicaton what type of data is accepted by the argument
 */
Value::Value(DefValueFun &&defValue, SetterFun &&setter, const std::string argHint)
  : _defaultVal( std::move(defValue) ),
    _setter( std::move(setter) ),
    _argHint(argHint)
{

}

/**
 * Calls the setter functor, with either the given argument or the optional argument
 * if the \a in parameter is null. Additionally it checks if the argument was already seen
 * before and fails if that \a Repeatable flag is not set
 */
bool Value::set(CommandOption *opt, const boost::optional<std::string> in)
{
  if ( _wasSet && !(opt->flags & CommandOption::Repeatable)) {
    std::cerr << "Option "<<opt->name<<" can only be used once"<< std::endl;
    return false;
  }

  _wasSet = true;

  if ( !in && opt->flags & CommandOption::OptionalArgument ) {
      auto optVal = _defaultVal();
      if (!optVal)
        return false;
      return _setter( opt, optVal );
  } else if ( in || (!in && (opt->flags & CommandOption::ArgumentTypeMask) == CommandOption::NoArgument) )  {
    return _setter(opt, in);
  }
  return false;
}

/**
 * Returns the default value represented as string, or a empty
 * boost::optional if no default value is given
 */
boost::optional<std::string> Value::defaultValue() const
{
  return _defaultVal();
}

/**
 * returns the hint for the input a command accepts,
 * used in the help
 */
std::string Value::argHint() const
{
  return _argHint;
}

/**
 * Returns a \sa Value instance handling flags taking a string parameter
 */
Value StringType(std::string *target, const boost::optional<const char *> &defValue, const char *hint) {
  return Value (
    [defValue]() ->  boost::optional<std::string>{
      if (!defValue || *defValue == nullptr)
        return boost::optional<std::string>();
      return std::string(*defValue);
    },
    [target]( CommandOption *, const boost::optional<std::string> &in ){
      if (in)
        *target = *in;
      return in.operator bool();
    },
    hint
  );
}

/**
 * Returns a \sa Value instance handling flags taking a int parameter
 */
Value IntType(int *target, const boost::optional<int> &defValue) {
  return Value (
        [defValue]() -> boost::optional<std::string>{
          if(defValue) {
            return std::to_string(*defValue);
          } else
            return boost::optional<std::string>();
        },

        [target]( CommandOption *opt, const boost::optional<std::string> &in ) -> bool{
          if ( !in )
            return false;

          try {
            *target = std::stoi( *in );
          } catch ( const std::invalid_argument &e ) {
            std::cerr << "Argument: " << opt->name << " is invalid."<<std::endl;
            return false;
          } catch ( const std::out_of_range &e) {
            std::cerr << "Argument: " << opt->name << " is out of range."<<std::endl;
            return false;
          } catch ( ... ) {
            std::cerr << "Unknown error while handling the argument " << opt->name << std::endl;
            return false;
          }
          return true;
        },
        "NUMBER"
  );
}

/**
 * Creates a boolean flag. Can either set or unset a boolean value controlled by \a store.
 * The value in \a defVal is only used for generating the help
 */
Value BoolType(bool *target, StoreFlag store, const boost::optional<bool> &defVal) {
  return Value (
    [defVal]() -> boost::optional<std::string>{
      if (!defVal)
        return boost::optional<std::string>();
      return std::string( (*defVal) ? "true" : "false" );
    },
   [target, store]( CommandOption *, const boost::optional<std::string> &){
      *target = (store == StoreTrue);
      return true;
    }
  );
}

/**
 * Parses the command line arguments based on \a options.
 * \returns The first index in argv that was not parsed
 */
int parseCLI(const int argc, char * const *argv, const std::vector<CommandGroup> &options)
{
  // the short options string as used int getopt
  // + - do not permute, stop at the 1st nonoption, which is the command
  // : - return : to indicate missing arg, not ?
  std::string shortopts( "+:" );

  // the set of long options
  std::vector<struct option> longopts;

  //build a complete list and a long and short option index so we can
  //easily get to the CommandOption
  std::vector<CommandOption> allOpts;
  std::map<std::string, int> longOptIndex;  //we do not actually need that index other than checking for dups
  std::map<char, int>        shortOptIndex;

  for ( const CommandGroup &grp : options ) {
    for ( const CommandOption &currOpt : grp.options ) {
      allOpts.push_back( currOpt );

      int allOptIndex = allOpts.size() - 1;

      if ( currOpt.flags & CommandOption::RequiredArgument && currOpt.flags &  CommandOption::OptionalArgument ) {
        throw Exception("Argument can either be Required or Optional");
      }

      if ( currOpt.name ) {
        if ( !longOptIndex.insert( { currOpt.name, allOptIndex } ).second) {
          throw Exception("Duplicate long option <insertnamehere>");
        }
        appendToLongOptions( currOpt, longopts );
      }

      if ( currOpt.shortName ) {
        if ( !shortOptIndex.insert( { currOpt.shortName, allOptIndex } ).second) {
          throw Exception("Duplicate short option <insertnamehere>");
        }
        appendToOptString( currOpt, shortopts );
      }
      allOptIndex++;
    }
  }

  //the long options always need to end with a set of zeros
  longopts.push_back({0, 0, 0, 0});

  //setup getopt
  opterr = 0; 			// we report errors on our own
  optind = 0;                   // start on the first arg

  while ( true ) {

    int option_index = -1;      //index of the last found long option, same as in allOpts
    int optc = getopt_long( argc, argv, shortopts.c_str(), longopts.data(), &option_index );

    if ( optc == -1 )
      break;

    switch ( optc )
    {
      case '?': {
        // wrong option in the last argument
        std::cerr << "Unknown option " << "'";

        // last argument was a short option
        if ( option_index == -1 && optopt)
          std::cerr << (char) optopt;
        else
          std::cerr << argv[optind - 1];

        std::cerr << "'" << std::endl;

        // tell the caller there have been unknown options encountered
        // result["_unknown"].push_back( "" );
        break;
      }
      case ':': {
        std::cerr << "Missing argument for " << argv[optind - 1] << std::endl;
        // result["_missing_arg"].push_back( "" );
        break;
      }
      default: {
        int index = -1;
        if ( option_index == -1 ) {
          //we have a short option
          auto it = shortOptIndex.find( (char) optc );
          if ( it != shortOptIndex.end() ) {
            index = it->second;
          }
        } else {
          //we have a long option
          index = option_index;
        }

        if ( index >= 0 ) {

          boost::optional<std::string> arg;
          if ( optarg && strlen(optarg) ) {
            arg = std::string(optarg);
          }

          allOpts[index].value.set( &allOpts[index], arg);
        }

        break;
      }
    }
  }
  return optind;
}

Exception::Exception(const std::string what_r) : _what (what_r)
{ }

const char *Exception::what() const noexcept
{
  return _what.c_str();
}

/**
 * Renders the \a options help string
 */
void renderHelp(const std::vector<CommandGroup> &options)
{
  for ( const CommandGroup &grp : options ) {
    std::cout << grp.name << ":" << std::endl << std::endl;
    for ( const CommandOption &opt : grp.options ) {
      if ( opt.shortName )
        std::cout << "-" << opt.shortName << ", ";
      else
        std::cout << "    ";

      std::cout << "--" << opt.name;

      std::string argSyntax = opt.value.argHint();
      if ( argSyntax.length() ) {
        if ( opt.flags & GnuFlag::CommandOption::OptionalArgument )
          std::cout << "[=";
        else
          std::cout << " <";
        std::cout << argSyntax;
        if ( opt.flags & GnuFlag::CommandOption::OptionalArgument )
          std::cout << "]";
        else
          std::cout << ">";
      }

      std::cout << "\t" << opt.help;

      auto defVal = opt.value.defaultValue();
      if ( defVal )
        std::cout << " Default: " << *defVal;

      std::cout << std::endl;

    }
    std::cout<<std::endl;
  }
}

}


