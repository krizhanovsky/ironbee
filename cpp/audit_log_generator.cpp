#include "audit_log_generator.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#include <boost/regex.hpp>
#pragma clang diagnostic pop
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <stdexcept>
#include <fstream>

using namespace std;

namespace IronBee {
namespace CLI {

AuditLogGenerator::AuditLogGenerator(
  const std::string& path,
  on_error_t on_error
 ) :
  m_on_error( on_error ),
  m_input( boost::make_shared<ifstream>( path.c_str() ) ),
  m_parser( *m_input )
{
  if ( ! *m_input ) {
    throw runtime_error( "Error reading " + path );
  }
}

bool AuditLogGenerator::operator()( input_t& out_input )
{
  AuditLog::Entry e;

  bool have_entry = false;
  bool result;
  while ( ! have_entry ) {
    try {
      result = m_parser( e );
    }
    catch ( const exception& e ) {
      if ( m_on_error.empty() ) {
        throw;
      }
      if ( m_on_error( e.what() ) ) {
        m_parser.recover();
      }
    }
    if ( ! result ) {
      return false;
    }
    have_entry = true;
  }

  // Extract connection information.
  static const boost::regex section_a(
    R"re(([0-9.]+) (\d+) ([0-9.]+) (\d+)$)re"
  );
  boost::smatch match;
  const auto& A = e["A"];
  if ( regex_search( A, match, section_a ) ) {
    out_input.src_ip.data   = A.c_str() + match.position( 1 );
    out_input.src_ip.length = match.length( 1 );

    out_input.src_port = boost::lexical_cast<uint16_t>( match.str( 2 ) );

    out_input.dst_ip.data   = A.c_str() + match.position( 3 );
    out_input.dst_ip.length = match.length( 3 );

    out_input.dst_port = boost::lexical_cast<uint16_t>( match.str( 4 ) );
  } else {
    throw runtime_error(
      "Could not parse connection information: " + A
    );
  }

  out_input.request  = buffer_t( e["B"] );
  out_input.response = buffer_t( e["F"] );

  return true;
}

} // CLI
} // IronBee
