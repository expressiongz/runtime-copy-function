#include <iostream>
#include <Windows.h>
#include <thread>
#include <vector>
#include <array>

void* copy_function( void* function_address )
{
	// MSVC specific way of copying functions. not standardized.
	const auto function_instrs = [ ]( void* function_address ) -> std::vector< std::uint8_t >
	{
		std::vector< std::uint8_t > function_bytes{ };
		auto byte = static_cast< std::uint8_t* >( function_address );

		do 
		{
			function_bytes.push_back( *byte );
			++byte;
		} while( *reinterpret_cast< std::uint32_t* >( byte ) != 0xCCCCCCCC );

		return function_bytes;
	}( function_address );

	// throw exception in case virtualalloc fails
	const auto allocated_func_mem = VirtualAlloc( nullptr, function_instrs.size( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	if( !allocated_func_mem )
		throw std::runtime_error( "failed to allocate memory for function" );
	
	// relative calls address re-calculation adjusted for the "new" memory location of the function.
	for( auto it = function_instrs.begin( ); it < function_instrs.end( ); ++it )
	{
		if( *it == 0xE9 || *it == 0xE8 )
		{
			// getting the offset of bytes from the beginning of the function 
			// so we can add it later to get the next instruction address
			// of both the original function and our new function.

			const auto dist = std::distance( function_instrs.begin( ), it );
			// old offset used by the old function to make a relative CALL or JMP  
			const auto o_rel_offset = *reinterpret_cast< std::uint32_t* >( it._Ptr + 1 );
			// the address of the instruction after the relative CALL instruction 
			const auto o_next_instr = reinterpret_cast< std::uint32_t >( function_address ) + dist + 5;
			// absolute address of the callee
			const auto callee_abs_address = o_next_instr + o_rel_offset;
			// same as before except it's the next instruction of the new function
			const auto next_instr = reinterpret_cast< std::uint32_t >( allocated_func_mem ) + dist + 5;
			// relative address calculation
			const auto rel_addr = callee_abs_address - next_instr;
			// setting the new relative address operand value
			*reinterpret_cast< std::uint32_t* >( it._Ptr + 1 ) = rel_addr;
		}
	}
	
	// copying the new function with the fixed relative addresses to our allocated memory for it
	std::memcpy( allocated_func_mem, function_instrs.data( ), function_instrs.size( ) );
	return allocated_func_mem;

}

void main_thread( HMODULE dll_module )
{
	const auto base = reinterpret_cast< std::uint32_t >( GetModuleHandleA( nullptr ) );
	constexpr auto function_rva = 0x1020;
	
	const auto new_func_address = copy_function( reinterpret_cast< void* >( base + function_rva ) );
	const auto new_func = static_cast< void( __cdecl * )( const char* ) >( new_func_address );
	new_func( "exprssn is godly\n" );
	FreeLibrary( dll_module );
}


bool __stdcall DllMain( HMODULE dll_module, const std::uint32_t reason_for_call, void* )
{
	if( reason_for_call == DLL_PROCESS_ATTACH )
		std::thread{ main_thread, dll_module }.detach( );
	return true;
}