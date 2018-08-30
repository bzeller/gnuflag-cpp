#ifndef GNUFLAG_H
#define GNUFLAG_H

#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include <exception>

#include <boost/optional.hpp>

namespace GnuFlag {

  struct CommandOption;


  class Value {

  public:
    using DefValueFun = std::function<boost::optional<std::string>()>;
    using SetterFun   = std::function<bool ( CommandOption *, const boost::optional<std::string> &in)>;

    Value ( DefValueFun &&defValue, SetterFun &&setter, const std::string argHint = std::string() );
    bool set ( CommandOption * opt, const boost::optional<std::string> in );
    boost::optional<std::string> defaultValue ( ) const;
    std::string argHint () const;

  private:
    bool _wasSet = false;
    DefValueFun _defaultVal;
    SetterFun _setter;
    std::string _argHint;
  };

  Value StringType ( std::string *target, const boost::optional<const char *> &defValue = boost::optional<const char *> (), const char * hint = "STRING" );
  Value IntType    ( int *target, const boost::optional<int> &defValue = boost::optional<int>()  );

  template <class Container>
  Value StringContainerType ( Container *target, const char * hint = "STRING"  ) {
    return Value (
          []() -> boost::optional<std::string> { return boost::optional<std::string>(); },
          [target] ( CommandOption *, const boost::optional<std::string> &in ) {
            if (!in) return false; //value required
            target->push_back(*in);
            return true;
          },
          hint
    );
  }


  enum StoreFlag : int{
    StoreFalse,
    StoreTrue
  };
  Value BoolType   ( bool *target, StoreFlag store = StoreTrue, const boost::optional<bool> &defValue = boost::optional<bool>()  );

  class Exception : public std::exception
  {
  public:
    Exception( const std::string what_r );
    const char *what() const noexcept override;
  private:
    std::string _what;
  };

  struct CommandOption
  {
    enum ArgFlags : int {
      NoArgument       = 0,
      RequiredArgument = 0x01,
      OptionalArgument = 0x02,
      ArgumentTypeMask = 0x0F,

      Repeatable       = 0x10, // < the argument can be repeated
    };

    const char *name;
    const char  shortName;
    const int flags;
    Value value;
    const std::string help;
  };

  struct CommandGroup
  {
    const std::string name;
    std::vector<CommandOption> options;
  };

  int parseCLI ( const int argc, char * const *argv, const std::vector<CommandGroup> &options );
  void renderHelp( const std::vector<CommandGroup> &options );

}


#endif // GNUFLAG_H
