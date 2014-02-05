
#include <set>

#include <boost/function.hpp>
#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp>
#include <boost/program_options.hpp>

namespace {

void show_help(
               std::ostream& out_,
               const boost::program_options::options_description& desc_
               )
{
  out_
    << "Usage:\n"
    << "  eo-read <options>\n"
    << "\n"
    << desc_ << std::endl;
}

boost::wave::token_id as_id(boost::wave::token_id x) { return x; }

struct function
{
  std::string enum_base, enum_offset;
  std::string name;
  std::vector<std::string> params;
};

template <typename Token>
bool is_space(Token& token)
{
  return (as_id(token) & boost::wave::TokenTypeMask) == boost::wave::WhiteSpaceTokenType;
}

template <typename Iterator, typename U>
bool skip_space(Iterator& token, U& minimum_size)
{
  if(is_space(*token))
    {
      ++minimum_size;
      ++token;
    }
  return true;
}

template <typename T>
struct hooks : boost::wave::context_policies::eat_whitespace<T>
{
  std::vector<std::string> id_macros;
  std::vector<function>* functions;

  hooks(std::vector<function>& functions)
    : functions(&functions) {}

 template <
        typename ContextT, typename TokenT, typename ParametersT, 
        typename DefinitionT
    >
    void defined_macro(ContextT const& ctx, 
        TokenT const &name, bool is_functionlike,
        ParametersT const &parameters, DefinitionT const &definition,
        bool is_predefined)
  {
    if(is_functionlike && parameters.size() == 1)
      {
        typename DefinitionT::const_iterator first = definition.begin()
          , last = definition.end();
        
        std::size_t minimum_size = 5;
        if(definition.size() >= minimum_size
           && as_id(*first++) == boost::wave::T_LEFTPAREN
           && as_id(*first++) == boost::wave::T_IDENTIFIER
           && skip_space(first, minimum_size) && definition.size() >= minimum_size
           && as_id(*first++) == boost::wave::T_PLUS
           && skip_space(first, minimum_size) && definition.size() >= minimum_size
           && as_id(*first++) == boost::wave::T_IDENTIFIER
           && as_id(*first) == boost::wave::T_RIGHTPAREN
           )
          {
            id_macros.push_back(name.get_value().c_str());
            return;
          }
      }

    std::size_t minimum_size = 4 + parameters.size()*7;
    if(is_functionlike && definition.size() >= minimum_size)
      {
        typename DefinitionT::const_iterator first = definition.begin()
          , last = definition.end();

        TokenT enum_base, enum_offset;

        if(as_id(*first) == boost::wave::T_IDENTIFIER
           && std::find(id_macros.begin(), id_macros.end(), first->get_value().c_str())
           != id_macros.end()
           && (enum_base = *first, true)
           && skip_space(++first, minimum_size) && definition.size() >= minimum_size
           && as_id(*first++) == boost::wave::T_LEFTPAREN
           && skip_space(first, minimum_size) && definition.size() >= minimum_size
           && as_id(*first) == boost::wave::T_IDENTIFIER
           && (enum_offset = *first, true)
           && skip_space(++first, minimum_size) && definition.size() >= minimum_size
           && as_id(*first++) == boost::wave::T_RIGHTPAREN
           )
          {
            function f = {std::string(enum_base.get_value().begin(), enum_base.get_value().end())
                          , std::string(enum_offset.get_value().begin(), enum_offset.get_value().end())
                          , std::string(name.get_value().begin(), name.get_value().end())};
            for(typename ParametersT::const_iterator param_first = parameters.begin()
                  , param_last = parameters.end(); param_first != param_last; ++param_first)
              {
                std::vector<TokenT> type_tokens;
                
                if(skip_space(first, minimum_size) && definition.size() >= minimum_size
                   && skip_space(first, minimum_size) && definition.size() >= minimum_size
                   && as_id(*first++) == boost::wave::T_COMMA
                   && skip_space(first, minimum_size) && definition.size() >= minimum_size
                   && as_id(*first) == boost::wave::T_IDENTIFIER
                   && first->get_value() == "EO_TYPECHECK"
                   && skip_space(++first, minimum_size) && definition.size() >= minimum_size
                   && as_id(*first++) == boost::wave::T_LEFTPAREN
                   )
                  {
                    --minimum_size;
                    while(first != definition.end()
                          && as_id(*first) != boost::wave::T_COMMA)
                      {
                        ++minimum_size;
                        type_tokens.push_back(*first++);
                      }

                    if(first != definition.end() && definition.size() >= minimum_size
                       && as_id(*first) == boost::wave::T_COMMA
                       && skip_space(++first, minimum_size) && definition.size() >= minimum_size
                       && as_id(*first) == boost::wave::T_IDENTIFIER
                       && first->get_value() == param_first->get_value()
                       && (++first, true)
                       && skip_space(first, minimum_size)
                       && as_id(*first++) == boost::wave::T_RIGHTPAREN
                       )
                      {
                        std::string type;
                        for(typename std::vector<TokenT>::const_iterator token_first = type_tokens.begin()
                              , token_last = type_tokens.end(); token_first != token_last;++token_first)
                          {
                            type.insert(type.end(), token_first->get_value().begin()
                                        , token_first->get_value().end());
                          }
                        
                        f.params.push_back(type);
                      }
                    else
                      std::cout << "Failed param name " << first->get_value() << std::endl;
                  }
                else
                  std::cout << "Failed sig " << first->get_value()
                            << " minimum_size " << minimum_size 
                            << " size " << definition.size()
                            << std::endl;
              }
            functions->push_back(f);
          }
      }
  }
};

}

int main(int argc, const char* argv[])
{
  std::string filename;
  std::vector<std::string> include_path;
  std::vector<std::string> macros;
  {
  using boost::program_options::options_description;
  using boost::program_options::variables_map;
  using boost::program_options::store;
  using boost::program_options::notify;
  using boost::program_options::parse_command_line;
  using boost::program_options::value;

  options_description desc("Options");
  desc.add_options()
    ("help", "Display help")
    ("include,I", value(&include_path), "Additional include directory")
    ("define,D", value(&macros), "Additional macro definitions")
    ("file,f", value(&filename), "Header filename")
    ;

  try
  {
    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help"))
    {
      show_help(std::cout, desc);
      return 0;
    }
  }
  catch (const std::exception& e_)
  {
    std::cerr << e_.what() << "\n\n";
    show_help(std::cerr, desc);
    return 0;
  }
  }

  std::ifstream instream(filename.c_str());

  std::string input(std::istreambuf_iterator<char>(instream.rdbuf()),
                    std::istreambuf_iterator<char>());

  typedef boost::wave::cpplexer::lex_iterator<
    boost::wave::cpplexer::lex_token<> >
    lex_iterator_type;
  typedef boost::wave::context<
    std::string::iterator, lex_iterator_type
    , boost::wave::iteration_context_policies::load_file_to_string
    , hooks<boost::wave::cpplexer::lex_token<> > >
    context_type;
  
  std::vector<function> functions;
  context_type ctx(input.begin(), input.end(), filename.c_str(), functions);

  ctx.set_language(boost::wave::support_c99);

  for(std::vector<std::string>::const_iterator
        first = include_path.begin(), last = include_path.end()
        ;first != last;++first)
    {
      ctx.add_include_path(first->c_str());
      ctx.add_sysinclude_path(first->c_str());
      ctx.add_sysinclude_path("/usr/include");
      ctx.add_sysinclude_path("/usr/include/c++/4.8.2/tr1");
      ctx.add_sysinclude_path("/usr/include/c++/4.8.2");
      ctx.add_sysinclude_path("/usr/include/linux");
    }

  ctx.add_macro_definition("EFL_EO_API_SUPPORT");

  context_type::iterator_type first = ctx.begin();
  context_type::iterator_type last = ctx.end();  

  try
    {
      while (first != last)
        {
          ++first;
        }  
    }
  catch(boost::wave::preprocess_exception const& e)
    {
      std::cout << "Error: " << e.description() << std::endl;
    }

  std::cout << "Found " << functions.size() << " functions" << std::endl;
}
