set (UBITectSourceCodes
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
add_library (UBITectObj OBJECT ${UBITectSourceCodes})
add_library (UBITect SHARED $<TARGET_OBJECTS:UBITectObj>)
add_library (UBITectStatic STATIC $<TARGET_OBJECTS:UBITectObj>)
llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support)
# Build executable ubitect.
set (EXECUTABLE_OUTPUT_PATH ${UBITECT_BINARY_DIR})
link_directories (${UBITECT_BINARY_DIR}/lib)
#project (ubitect C CXX)
add_executable(ubitect ${UBITectSourceCodes})
target_link_libraries(ubitect 
  ${llvm_libs}
  LLVMAsmParser 
  LLVMSupport 
  LLVMCore 
  LLVMAnalysis
  LLVMIRReader
  LLVMTransformUtils
  UBITectStatic
  )
