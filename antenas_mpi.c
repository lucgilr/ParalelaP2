/**
 * Computación Paralela (curso 1516)
 *
 * Colocación de antenas
 * Versión paralela
 *
 * @author Lucía Gil
 * @author Jorge Hernández
 */


// Includes generales
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <mpi.h>

// Include para las utilidades de computación paralela
#include "cputils.h"


/**
 * Estructura antena
 */
typedef struct {
	int fila;
	int columna;
} Antena;


/**
 * Macro para acceder a las posiciones del mapa
 */
#define m(y,x) mapa[ (y * cols) + x ]


/**
 * Función de ayuda para imprimir el mapa
 */
void print_mapa(int * mapa, int offset, int tam, int cols, Antena * a, int rank){


	if(tam > 50 || cols > 30){
		printf("Mapa muy grande para imprimir\n");
		return;
	};

	#define ANSI_COLOR_RED     "\x1b[31m"
	#define ANSI_COLOR_GREEN   "\x1b[32m"
	#define ANSI_COLOR_RESET   "\x1b[0m"

	//printf("Mapa [%d,%d]\n",rows,cols);
	printf("Porción del proceso %d\n",rank);
	for(int i=0; i<tam; i++){
		for(int j=0; j<cols; j++){

			int val = m(i,j);

			if(val == 0){
				if(a != NULL && a->columna == j && a->fila == offset+i){
					printf( ANSI_COLOR_RED "   A"  ANSI_COLOR_RESET);
				} else { 
					printf( ANSI_COLOR_GREEN "   A"  ANSI_COLOR_RESET);
				}
			} else {
				printf("%4d",val);
			}
		}
		printf("\n");
	}
	printf("\n");
}



/**
 * Distancia de una antena a un punto (y,x)
 * @note Es el cuadrado de la distancia para tener más carga
 */
int manhattan(Antena a, int fila, int columna){
	int dist = abs(a.fila - fila) + abs(a.columna - columna);
	return dist * dist;
}



/**
 * Actualizar el mapa con la nueva antena
 */
void actualizar(int * mapa, int offset, int tam, int cols, Antena antena){
	
	for(int i=0; i<tam; i++){
		for(int j=0; j<cols; j++){
			int nuevadist = manhattan(antena,offset + i,j);
			if(nuevadist < m(i,j)){
				m(i,j) = nuevadist;
			}
		}
	}
}

/* Cada proceso buscará en su trozo de la matriz donde colocar su antena
 * y lo guardará en un vector con tres enteros que indican:
 *  - fila de la nueva antena.
 *  - columna de la nueva antena.
 *  - distancia actual en esa posición sin antena
 */

typedef int AntenaNueva[3]; 

/* Para el calculo de la nueva antena a colocar, podemos hacer un 
 * AllReduce con las posibles antenas de cada uno de los procesos.
 */

void max_nueva_antena(void *invec, void *outvec, int *len, MPI_Datatype *data){
	AntenaNueva * in = (AntenaNueva *) invec;
	AntenaNueva * out = (AntenaNueva *) outvec;
	int i,j;
	for(i = 0; i < *len; i++){
		// Si la distancia es mayor es el nuevo máximo, pero si es igual
		if(in[i][2] > out[i][2])
			for(j = 0; j < 3;j++) out[i][j]=in[i][j];
		// Si la distancia es la misma, nos quedamos con el que tenga menor fila
		// y si estan en la misma fila, el que esta mas a la izquierda en columnas
		else if( in[i][2] == out[i][2]){
			if(in[i][0] < out[i][0] || (in[i][0] == out[i][0] && in[i][1] < out[i][1]))
				for(j = 0; j < 3;j++) out[i][j]=in[i][j];
		}
	}
}

/**
 * Función principal
 */
int main(int nargs, char ** vargs){

	//Para distinguir cada uno de los procesos
	int rank, size;
	//Inicialización de MPI
	MPI_Init(&nargs,&vargs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	//Definición de tipo y operación para la reducción de búsqueda del máximo
	MPI_Datatype MPI_ANTENA;
	MPI_Type_contiguous(3, MPI_INT, &MPI_ANTENA);
	MPI_Type_commit(&MPI_ANTENA);

	MPI_Op MPI_MAX_ANTENA;

	// Comprobar número de argumentos
	if(nargs < 7){
		fprintf(stderr,"Uso: %s rows cols distMax nAntenas x0 y0 [x1 y1, ...]\n",vargs[0]);
		return -1;
	}
	MPI_Op_create(&max_nueva_antena, 0, &MPI_MAX_ANTENA);

	//
	// 1. LEER DATOS DE ENTRADA
	//

	// Leer los argumentos de entrada
	int rows = atoi(vargs[1]);
	int cols = atoi(vargs[2]);
	int distMax = atoi(vargs[3]);
	int nAntenas = atoi(vargs[4]);

	//printf("nAntenas: %d\n", nAntenas);
	if(nAntenas<1 || nargs != (nAntenas*2+5)){
		fprintf(stderr,"Error en la lista de antenas\n");
		return -1;
	}
	// Reservar memoria para las antenas
	Antena * antenas = malloc(sizeof(Antena) * (size_t) nAntenas);
	if(!antenas){
		fprintf(stderr,"Error al reservar memoria para las antenas inicales\n");
		return -1;
	}	
	// Leer antenas
	for(int i=0; i<nAntenas; i++){
		antenas[i].columna = atoi(vargs[5+i*2]);
		antenas[i].fila = atoi(vargs[6+i*2]);

		if(antenas[i].fila<0 || antenas[i].fila>=rows || antenas[i].columna<0 || antenas[i].columna>=cols ){
			fprintf(stderr,"Antena #%d está fuera del mapa\n",i);
			return -1;
		}
	}

	/* Mensaje
	printf("Calculando el número de antenas necesarias para cubrir un mapa de"
		   " (%d x %d)\ncon una distancia máxima no superior a %d "
		   "y con %d antenas iniciales\n\n",rows,cols,distMax,nAntenas);
	*/

	//
	// 2. INICIACIÓN
	//

	// Medir el tiempo
	double tiempo = cp_Wtime();

	// Repartición de las filas en función del número del proceso que seas:
	int offset;
	int lastrow;
	int tam;
	if(rows >= size){
		offset = rank * rows / size;
		lastrow = ((rank + 1) * rows / size)-1;
		tam = lastrow - offset + 1;
	
	}
	else{
		tam = 1;
		if(rank < rows){
			offset = rank;
			lastrow = rank;
		}
		else{
			offset = rows-1;
			lastrow = rows-1;
		}
	}
	
	//printf("RANK: %d\tFila inicial: %d\tFila final: %d Tam: %d\n",rank,offset,lastrow,tam);
	int * mapa = malloc((size_t) (tam*cols) * sizeof(int) );

	// Iniciar el mapa con el valor MAX INT
	for(int i=0; i<(tam*cols); i++){
		mapa[i] = INT_MAX;
	}

	// Colocar las antenas iniciales
	for(int i=0; i<nAntenas; i++){
		actualizar(mapa, offset, tam, cols,antenas[i]);
	}

	/*print_mapa(mapa,offset,tam,cols,NULL,rank);
	getchar();
	system("clear");*/
	
	//
	// 3. CALCULO DE LAS NUEVAS ANTENAS
	//

	// Contador de antenas
	int nuevas = 0;
	
	while(1){

		// Calcular la nueva antena en la porción del mapa asignado a este proceso:
		AntenaNueva max;
		AntenaNueva global_max;
		max[0] = offset;
		max[1] = 0;
		max[2] = 0;
		for(int i=0; i<tam; i++)
			for(int j=0; j<cols; j++)
				if(m(i,j)>max[2]){
					max[0] = offset+i;
					max[1] = j;
					max[2] = m(i,j);
				}
		// Compartimos al resto de procesos la antena encontrada, con AllReduce:
		MPI_Allreduce(&max, &global_max, 1, MPI_ANTENA, MPI_MAX_ANTENA, MPI_COMM_WORLD);				
		
		// Salimos si ya hemos cumplido el maximo
		if (global_max[2] <= distMax)
			break;	
		
		// Incrementamos el contador
		nuevas++;
		
		// Calculo de la nueva antena y actualización del mapa
		Antena antena = {global_max[0],global_max[1]};
		actualizar(mapa,offset,tam,cols,antena);
		/*if(rank == 0)
			printf("NEW: %d %d\n",antena.fila, antena.columna);
		print_mapa(mapa,offset,tam,cols,NULL,rank);
		getchar();
		system("clear");*/
	}

	// Debug
#ifdef DEBUG
	print_mapa(mapa,offset,tam,cols,NULL,rank);
#endif

	//
	// 4. MOSTRAR RESULTADOS
	//

	// tiempo
	tiempo = cp_Wtime() - tiempo;	

	// Salida
	if(rank == 0){
		printf("Result: %d\n",nuevas);
		printf("Time: %f\n",tiempo);
	}
	//Liberación de recursos:
	MPI_Op_free(&MPI_MAX_ANTENA);
	MPI_Type_free(&MPI_ANTENA);
	MPI_Finalize();

	return 0;
}



