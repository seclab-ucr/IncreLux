set (IncreLuxSourceCodes
	UBIAnalysis.h
	UBIAnalysis.cpp
	#Points-to CG
	CallGraph.h
	CallGraph.cc
	Common.h
	Config.h
	PtsSet.h
	Helper.h
	Helper.cpp
	StructAnalyzer.h
	StructAnalyzer.cpp
	Annotation.h
	Annotation.cpp
	NodeFactory.h
	NodeFactory.cpp
	QualifierAnalysis.h
	QualifierAnalysis.cpp
	QualiInference.cpp
	QualifierCheck.cpp
	IOQualifierAnalysis.cpp
	IOQualifierCheck.cpp
	OOBQualifierAnalysis.cpp
	OOBQualifierCheck.cpp
	ConstraintCollect.cpp
	AAMap.cpp
	FuncProcess.cpp
	FunctionSummary.h
	#json.hpp
	json11.hpp
	json11.cpp
)

# Build libraries.
add_library (IncreLuxObj OBJECT ${IncreLuxSourceCodes})
add_library (IncreLux SHARED $<TARGET_OBJECTS:IncreLuxObj>)
add_library (IncreLuxStatic STATIC $<TARGET_OBJECTS:IncreLuxObj>)
llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support)
# Build executable ubitect.
set (EXECUTABLE_OUTPUT_PATH ${INCRELUX_BINARY_DIR})
link_directories (${INCRELUX_BINARY_DIR}/lib)
#project (ubitect C CXX)
add_executable(increlux ${IncreLuxSourceCodes})
target_link_libraries(increlux 
  ${llvm_libs}
  LLVMAsmParser 
  LLVMSupport 
  LLVMCore 
  LLVMAnalysis
  LLVMIRReader
  LLVMTransformUtils
  IncreLuxStatic
  )
