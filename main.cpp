#include <iostream>
#include "gnuflag.h"

int main( int argc, char *argv[])
{
  std::string myStringVar = "I was untouched";
  std::string optionalVar = "I'm optional";
  std::vector<std::string> stringVec;
  bool myFlag = false;
  int myInt = 10;

  std::vector<GnuFlag::CommandGroup> options {
    {"Default", {
        { "int", 'i', GnuFlag::CommandOption::RequiredArgument, GnuFlag::IntType( &myInt, myInt) , "Set the Int value." },
        { "bool", 'b', GnuFlag::CommandOption::NoArgument , GnuFlag::BoolType( &myFlag, GnuFlag::StoreTrue, myFlag) , "Enable the bool switch." }
      }
    }, { "Extended", {
        { "string", 's', GnuFlag::CommandOption::RequiredArgument, GnuFlag::StringType( &myStringVar, myStringVar.c_str() ) , "Set the String value." },
        { "ostring", 'o', GnuFlag::CommandOption::OptionalArgument | GnuFlag::CommandOption::Repeatable, GnuFlag::StringType( &optionalVar, "Seen, i was seen") , "Set the optional String value." },
        { "cstring", 'c', GnuFlag::CommandOption::RequiredArgument | GnuFlag::CommandOption::Repeatable, GnuFlag::StringContainerType( &stringVec ) , "Add value to list of strings." }
      }
    }
  };

  std::cout << "My options: "<<std::endl;
  GnuFlag::renderHelp(options);

  int nextArgv = GnuFlag::parseCLI( argc, argv, options );

  std::cout << "Hello World!" << std::endl
            << "myStringVar: "<<myStringVar << std::endl
            << "optionalVar: "<<optionalVar << std::endl
            << "myFlag:      "<<myFlag << std::endl
            << "myInt:       "<<myInt  << std::endl;

  std::cout << "container:   "<< std::endl;
  for ( const std::string &str : stringVec ){
    std::cout << "\t"<< str << std::endl;
  }

  std::cout << "next in argv: "<<argv[nextArgv] << std::endl;
  return 0;
}
