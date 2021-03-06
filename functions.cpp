/*Universidade Federal do Rio de Janeiro
  Departamento de Engenharia Eletrôncia e de Computação
  Sistemas Operacionais - 2018.2
  Aluna: Fabiana Ferreira Fonseca
  DRE: 115037241*/

/*TRABALHO: IMPLEMENTAÇÃO DE UM SHELL PARA LINUX COM MULTIPLAS TELAS*/

#include <sstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include "consts.h"
#include "screen.h"
#include "functions.h"

#include <iostream>


/*Função que vai fazer o parser da string que vem da linha de comando, para pegar os argumentos inseridos*/
int getArgumentsFromCommand (char* command, char** arguments, std::string* pathOutput)
{
	char* subString;
	char inputCopy [BUFFER];
	unsigned counter = 0;
	unsigned flagOutput = 0;
	strcpy(inputCopy, command);
	subString = strtok(inputCopy, DELIMITER);
	/*Libera o espaço de memoria utilizado pelas strings dentro do array*/
	freeArray(arguments);
	while (subString != NULL)
	{
		if (strcmp(subString, ">") == 0)
			flagOutput = 1;

		else
		{
			/*Se tiver a flag, a string vai receber o resultado do parser atual da strtok*/
			if (flagOutput){
				/*Mudando o valor da variavel pathOutput e atribuindo o valor na posicao de memoria da subString*/
				pathOutput->assign(subString);
			}
			else
			{
				arguments[counter] = (char*)malloc((strlen(subString) + 1)*sizeof(char*));
				if (arguments[counter] != NULL)
					strcpy(arguments[counter],subString);
				else
					return -1;
				counter++;
			}
		}
		subString = strtok(NULL,DELIMITER);
	}
	/*NULL é adicionado no final para que a chamada ao exec funcione*/
	arguments[counter + 1] = NULL;
	if (!flagOutput)
		*pathOutput = "";
	return 0;
}

/*Funcao que irá liberar o espaço de memória utilizado com o malloc
do array que armazena os argumentos que são passados para a execv*/
void freeArray(char** arguments)
{
	unsigned i = 0;
	while (arguments[i] != NULL)
	{
		free(arguments[i]);
		/*Para marcar os ponteiros que foram liberados da memoria, atribuimos NULL ao mesmo*/
		arguments[i] = NULL;
		i++;
	}
}

void printUserGuide ()
{
  printf(MAGENTA_COLOR);
  printf("1. Comandos válidos são aqueles que estão na /bin ou 'exit' ou 'man fabits'. ou 'clear'\n");
  printf("2. Tratamentos de erro foram feitos para os casos acima,\n   assim como para argumentos vazios ou apenas um char.\n");
  printf("3. Para alterar o caminho de saída, basta digitar o comando e argumentos opcionais,\n   seguido de '> caminhoArquivo/nomeArquivo'\n");
  printf("4. Se especificado apenas o nome do arquivo, então será salvo no diretório atual\n");
  printf("5. O comando cd funciona com a limitação de nomes de diretórios que não tenham espaço,\n   por conta da lógica de parseamento de string\n");
  printf("6. O uso do signal SIGUSR1 é feito através de outra linha de comando e, quando executado,\n   encerra o processo filho que está executando\n");
	printf("7. Comando screen usado para criar uma nova tela\n");
	printf("7. screen remove <nome da tela> para remover\n");
	printf("8. screen switch <nome da tela> para alterar tela ativa\n");
	printf("9. screen list para listar telas existentes\n");
	printf("10. Comando monitor para mostrar uso de CPU e memoria\n");
  printf("Por fim, os arquivos encontram-se no GitHub, através do seguinte link: github.com/FabianaFerreira/Sistemas-Operacionais\n");
  printf(RESET_COLOR);
}

void printUserGreeting ()
{
  printf(CYAN_COLOR);
  printf("\n/******************************************************************************/\n");
  printf("/*------ FABITS SHELL: UM SHELL SIMPLIFICADO PARA SISTEMAS OPERACIONAIS ------*/\n");
  printf("/******************************************************************************/\n");
  printf(RESET_COLOR);
  printf("\n");
  printf("\n");
  printf(MAGENTA_COLOR);
  printf("Para maiores informações sobre como utilizá-lo, digite 'man fabits'\n");
  printf(RESET_COLOR);
  printf("\n");
  printf("\n");
}

void typeCommand ()
{
	printf(RED_COLOR);
	printf("Digite um comando válido\n");
	printf(RESET_COLOR);
}

void printInvalidCommand ()
{
	printf(RED_COLOR);
	printf("Comando não existente. Por favor, digite novamente.\n");
	printf(RESET_COLOR);
}

int checkError(int ret, std::string error)
{
  if (ret == -1) { perror(error.c_str()); exit(1); }
  return ret;
}

void writeAllToFifo(int fd, char * bytes, size_t nbyte)
{
  ssize_t written = 0;
  while(written < nbyte) {
    written += checkError(write(fd, bytes+written, nbyte-written), "Could not write to pipe");
  }
}

void write_str(int fd, std::string chars)
{
	char *string = new char[chars.size()+1];
 	strcpy(string,chars.c_str( ));
	writeAllToFifo(fd, string, chars.length());
}

std::vector<std::string> parseString (std::string str, char delimiter)
{
  std::vector<std::string> tokenVector;
  std::stringstream ss (str);
  std::string token;
  while (std::getline (ss, token, delimiter)) {
      tokenVector.push_back (token);
  }
  return tokenVector;
}

int parseLine(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

/*Funcao que le do arquivo que contem as informacoes de uso de CPU*/
double getCurrentUseOfCPU ()
{
    double percent;
    FILE* file;
    unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;
		static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;

    file = fopen(STATUS_CPU_PATH, "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow,
        &totalSys, &totalIdle);
    fclose(file);

    if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
        totalSys < lastTotalSys || totalIdle < lastTotalIdle){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else{
        total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
            (totalSys - lastTotalSys);
        percent = total;
        total += (totalIdle - lastTotalIdle);
        percent /= total;
        percent *= 100;
    }

    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;

    return percent;
}

/*Funcao que le do arquivo que contem as informacoes de uso de RAM*/
int getCurrentUseOfRAM ()
{
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result;
}
