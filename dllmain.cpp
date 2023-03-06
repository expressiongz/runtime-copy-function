#include <iostream>
#include <Windows.h>
#include <thread>
#include <vector>
#include <array>
#include <unordered_map>

std::vector< std::uint8_t > ret_function_bytes( void* address )
{
	auto byte = static_cast< std::uint8_t* >( address );
	std::vector< std::uint8_t > function_bytes{ };

	static const std::unordered_map< std::uint8_t, std::uint8_t > ret_bytes_map
	{
		{
			0xC2,
			0x03
		},
		{
			0xC3,
			0x01
		}
	};

	static const auto alignment_bytes = std::to_array< std::uint8_t >
	( 
		{ 0xCC, 0x90 } 
	);

	while( true )
	{
		function_bytes.push_back( *byte );

		for( const auto& ret_byte : ret_bytes_map )
		{
			const auto& [ opcode, opcode_sz ] = ret_byte;
			if( *byte == opcode )
			{
				for( const auto& alignment_byte : alignment_bytes )
				{
					if( *( byte + opcode_sz ) == alignment_byte )
						return function_bytes;
				}
			}
			
		}
		++byte;
	}
	
}

void* copy_function( void* function_address )
{
	const auto function_instrs = ret_function_bytes( function_address );

	// throw exception in case virtualalloc fails
	const auto allocated_func_mem = VirtualAlloc( nullptr, function_instrs.size( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	if( !allocated_func_mem )
		throw std::runtime_error( "failed to allocate memory for function" );
	
	// relative calls address re-calculation adjusted for the "new" memory location of the function.
	for( auto it = function_instrs.begin( ); it < function_instrs.end( ); ++it )
	{
		if( *it == 0xE9 || *it == 0xE8 )
		{
			const auto dist = std::distance( function_instrs.begin( ), it );
			const auto o_rel_offset = *reinterpret_cast< std::uint32_t* >( it._Ptr + 1 );
			const auto o_next_instr = reinterpret_cast< std::uint32_t >( function_address ) + dist + 5;
			const auto callee_abs_address = o_next_instr + o_rel_offset;
			const auto next_instr = reinterpret_cast< std::uint32_t >( allocated_func_mem ) + dist + 5;
			const auto rel_addr = callee_abs_address - next_instr;
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
	constexpr auto function_rva = 0xDEADBEEF;
	void* new_func_address{ nullptr };
	// Imagine we are copying a function which takes a c-string as a parameter.
	try {
		 new_func_address = copy_function( reinterpret_cast< void* >( base + function_rva ) );
	} catch (const std::runtime_error& e) {
		std::printf( "Failed to copy function!" );
		FreeLibrary( dll_module );
	}
	const auto new_func = static_cast< void( __cdecl * )( const char* ) >( new_func_address );
	std::printf( "%p\n", new_func );
	new_func( "exprssn is godly\n" );
	FreeLibrary( dll_module );
}


bool __stdcall DllMain( HMODULE dll_module, const std::uint32_t reason_for_call, void* )
{
	if( reason_for_call == DLL_PROCESS_ATTACH )
		std::thread{ main_thread, dll_module }.detach( );
	return true;
}
