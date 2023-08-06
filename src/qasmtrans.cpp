#include <memory>
#include <string>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream> 

#include "qasmtrans.hpp"
#include "chip.hpp"
#include "parser_util.hpp"
#include "qasm_parser.hpp"
#include "transpiler.hpp"

using namespace QASMTrans;

void print_help()
{
    //print the help function for all the options
    std::cout<<"Usage: ./qasmtrans [options]"<<std::endl;
    std::cout<<"Option            Description"<<std::endl;
    std::cout<<"-i                Input qasm circuit file"<<std::endl;
    std::cout<<"-c <backend>      Path to backend configuration json file"<<std::endl;
    std::cout<<"-limited          Run the transpiler with limited physical qubits usage"<<std::endl;
    std::cout<<"-backend_list     Print the available backends"<<std::endl;
    std::cout<<"-m <name>         Set the transpiler desired machine, default is Qiskit"<<std::endl;
    std::cout<<"-v <0/1/2>        Set the output level, default is 0"<<std::endl;
    std::cout<<"-o <path>         Set the output file, default is data/output_qasm_file/transpiled_modename_filename.qasm"<<std::endl;
    std::cout<<"-h                print the help function"<<std::endl;
}

int main(int argc, char **argv)
{
    bool run_with_limit = false;
    IdxType mode = 0;
    std::string mode_name = "ibmq";
    IdxType debug_level = 0;
    std::string output_path = "../data/output_qasm_file/";
    std::map<std::string, IdxType> machineQubits = {
        {"ibmq_toronto", 27},
        {"ibmq_jakarta", 7},
        {"ibmq_guadalupe", 16},
        {"ibm_seattle", 433},
        {"ibm_cairo", 27},
        {"ibm_brisbane", 127},
        {"dummy_ibmq12", 12},
        {"dummy_ibmq14", 14},
        {"dummy_ibmq15", 15},
        {"dummy_ibmq16", 16},
        {"dummy_ibmq30", 30},
        {"aspen_m3",80},
        {"h1_2",12},
        {"h1_1",20}
    };
    if (argc == 1) 
    {
        print_help();
        return 0;
    }
    else
    {
        if (cmdOptionExists(argv, argv + argc, "-h"))
        {
            print_help();
            return 0;
        }
        //! need a -m for different machine mode basis
        if (cmdOptionExists(argv, argv + argc, "-limited"))
        {
            run_with_limit = true;
        }
        if (cmdOptionExists(argv, argv + argc, "-v"))
        {
            debug_level = IdxType(std::stoi(getCmdOption(argv, argv + argc, "-v")));
            
        }
        if (cmdOptionExists(argv, argv + argc, "-o"))
        {
            output_path = std::string(getCmdOption(argv, argv + argc, "-o"));
            
        }
        if (cmdOptionExists(argv, argv + argc, "-backend_list"))
        {
            std::cout<<"The available backends are:"<<std::endl;
            std::cout<<"ibmq_toronto (27 qubits)"<<std::endl;
            std::cout<<"ibmq_jakarta (7 qubits)"<<std::endl;
            std::cout<<"ibmq_guadalupe (16 qubits)"<<std::endl;
            std::cout<<"ibm_seattle (433 qubits)"<<std::endl;
            std::cout<<"ibm_cairo (27 qubits)"<<std::endl;
            std::cout<<"ibm_brisbane (127 qubits)"<<std::endl;
            std::cout<<"aspen_m3 (80 qubits)"<<std::endl;
            std::cout<<"h1_2 (12 qubits)"<<std::endl;
            std::cout<<"h1_1 (20 qubits)"<<std::endl;
            std::cout<<"dummy_ibmq12 (12 qubits)"<<std::endl;
            std::cout<<"dummy_ibmq14 (14 qubits)"<<std::endl;
            std::cout<<"dummy_ibmq15 (15 qubits)"<<std::endl;
            std::cout<<"dummy_ibmq16 (16 qubits)"<<std::endl;
            std::cout<<"dummy_ibmq30 (30 qubits)"<<std::endl;
            std::cout<<"You can manually add new machine in json file at data/device"<<std::endl;
            return 0;
        }
        if (cmdOptionExists(argv, argv + argc, "-m"))
        {
            mode_name = std::string(getCmdOption(argv, argv + argc, "-m"));
            if (mode_name == "ibmq" || mode_name == "IBMQ") 
            {
                mode = 0;
            }
            else if (mode_name == "ionq" || mode_name == "IonQ") 
            {
                mode = 1;
            }
            else if (mode_name == "Quantinuum" || mode_name == "quantinuum") 
            {
                mode = 2;
            }
            else if (mode_name == "Rigetti" || mode_name == "rigetti")
            {
                mode = 3;
            }
            else
            {
                std::cout<<"Invalid mode name, please check"<<std::endl;
                return 0;
            }
        }
        if (cmdOptionExists(argv, argv + argc, "-i"))
        {
            const char *filename = getCmdOption(argv, argv + argc, "-i");
            if (!cmdOptionExists(argv, argv + argc, "-c"))
            {
                cerr << "Error: missing machine backend file via -c" << endl;
                return 1;
            }
            string backendpath = string(getCmdOption(argv, argv + argc, "-c")); 
            //================= Parsing ==================
            qasm_parser parser(filename);
            IdxType n_qubits = parser.num_qubits();
            shared_ptr<Circuit> circuit = make_shared<Circuit>(n_qubits);
            parser.loadin_circuit(circuit);
            shared_ptr<Chip> chip = constructChip(n_qubits, backendpath,
                    run_with_limit,debug_level);
            if (debug_level > 0)
            {
                cout << "======== QASMTrans ========" << endl;
                cout << "Input circuit: " << filename << " (" << n_qubits << " qubits)" << endl;
                cout << "Basis gate mode: " << mode_name<< endl;
                cout << "Backend (topology): " << backendpath
                     <<" ("<< chip->chip_qubit_num << " physical qubits)"<<endl;
                cout << "Limit mode: " << (run_with_limit?"True":"False") << endl; 
            }
            //================= Transpilation ==================
            if (circuit->is_empty())
            {
                cerr<<"Error: Circuit from " << filename <<" is empty"<<endl;
                return 1;
            }
            transpiler(circuit, chip, parser.get_list_cregs(),
                    debug_level, mode);
            //================= Write out ==================
            dumpQASM(circuit,filename,output_path,debug_level,mode);
            cout << "Saving to: " << output_path << endl;
            return 0;
        }
    }
    std::cout<<"Invalid Commend Line, Please Check"<<std::endl;
    print_help();
    return 0;
}


