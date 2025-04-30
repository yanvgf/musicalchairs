#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1); // Inicia com n-1 cadeiras, capacidade máxima n
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};








class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {
            for (int i = 1; i <= num_jogadores; ++i) {
                jogadores_ativos.push_back(i);
            }            
        }

    // Inicia uma nova rodada, removendo uma cadeira e ressincronizando o semáforo
    void iniciar_rodada() {
        
        std::lock_guard<std::mutex> lock(music_mutex);
        
        // Não remove cadeira se for a primeira rodada (número de jogadores ativos == num_jogadores)
        if (jogadores_ativos.size() < static_cast<std::vector<int>::size_type>(num_jogadores)) {
            
            --cadeiras;
        
            // Ao dar release no semáforo, permite-se que os jogadores tentem
            // ocupar as cadeiras novamente. Na prática, isso só reseta o semáforo,
            // pois nenhuma thread fica bloqueada nele (estou usando try_acquire() 
            // ao invés de acquire()).
            cadeira_sem.release(cadeiras);

            // A música precisa parar DEPOIS de liberar o semáforo,
            // pois é a música que faz os jogadores esperarem na variável de condição
            musica_parada.store(false);

            std::cout << "Próxima rodada com " << jogadores_ativos.size() << " jogadores e " 
                << cadeiras << " cadeiras.\n";
            std::cout << "A música está tocando... 🎵\n\n";
        } 
        // Na primeira rodada, não é necessário remover cadeira
        // e o semáforo já está na quantidade correta (n-1)
        else {
            musica_parada.store(false);
            std::cout << "Primeira rodada com " << jogadores_ativos.size() << " jogadores e " 
                << cadeiras << " cadeiras.\n";
            std::cout << "A música está tocando... 🎵\n\n";
        }
    }

    void parar_musica() {
        // Simula o momento em que a música para e notifica os jogadores via variável de condição

        // A variável de condição trava as threads que tentam ocupar uma cadeira (tentar_ocupar_cadeira())
        // até que a música pare (musica_parada==True). Quando o coordenador para a música, ele notifica
        // todos os jogadores que estão esperando na variável de condição, de forma que eles verifiquem
        // novamente se musica_parada==True e tentem ocupar uma cadeira.
        // 
        // Para modificar musica_parada, adquire-se o mutex
        std::lock_guard<std::mutex> lock(music_mutex);
        musica_parada.store(true);
        std::cout << "> A música parou! Os jogadores estão tentando se sentar...\n\n";
        music_cv.notify_all(); // Notifica todos os jogadores
    }

    void eliminar_jogador(int jogador_id) {

        // Encontra o jogador no vetor
        auto it = std::find(jogadores_ativos.begin(), jogadores_ativos.end(), jogador_id);
        // 
        // Remove o jogador
        jogadores_ativos.erase(it);
        std::cout << "Jogador P" << jogador_id << " ficou de pé e foi eliminado!\n";
    }

    int num_jogadores_ativos() {
        // Retorna o número de jogadores ativos
        return jogadores_ativos.size();
    }

private:
    std::vector<int> jogadores_ativos; // Ainda não foram eliminados 
    int num_jogadores;
    int cadeiras;
};








class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), ativo(true) {}

    // Tenta ocupar uma cadeira utilizando o semáforo contador quando a música para 
    // (aguarda pela variável de condição)
    void tentar_ocupar_cadeira() {
        // Pega o mutex
        std::unique_lock<std::mutex> lock(music_mutex);
        // 
        // A variável de condição vai liberar o mutex e bloquear a thread
        // até que a música pare. Quando a música parar, a thread do jogador
        // vai ser notificada e o mutex será adquirido novamente.
        music_cv.wait(lock, [] { return musica_parada.load(); });

        if (cadeira_sem.try_acquire()) {
            std::cout << "Jogador P" << id << " conseguiu uma cadeira!\n";
        } else {
            // Se não conseguiu, o jogador é eliminado
            ativo = false;
            jogo.eliminar_jogador(id);
        }
        printf("id: %d, ativo: %d\n", id, ativo);

        return;
    }

    // Aguarda a música parar usando a variavel de condicao
    void joga() {
        while (ativo) {    

            tentar_ocupar_cadeira();

            // Espera a próxima rodada antes de continuar
            while (musica_parada.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        }
    }

    bool esta_ativo() {
        // Retorna se o jogador está ativo ou não
        return ativo;
    }

private:
    int id;
    JogoDasCadeiras& jogo;
    bool ativo;
};







class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    // Começa o jogo, dorme por um período aleatório e
    // para a música, sinalizando os jogadores 
    void iniciar_jogo() {

        // Gera novas rodadas enquanto o número de jogadores ativos for maior que 1
        while (jogo.num_jogadores_ativos() > 1) {

            // Gera tempo aleatório pra música parar
            std::random_device rd; // Obtém uma seed aleatória do hardware, se disponível
            std::mt19937 gen(rd()); // Motor Mersenne Twister semeado com rd()
            std::uniform_int_distribution<> distrib(1000, 5000); // Distribuição uniforme entre 1000 e 5000 ms
            int tempo_espera_ms = distrib(gen); // Gera o tempo de espera

            jogo.iniciar_rodada();

            // Dorme por [tempo_espera_ms] ms antes de parar a música
            std::this_thread::sleep_for(std::chrono::milliseconds(tempo_espera_ms));

            jogo.parar_musica();
        }
    }

private:
    JogoDasCadeiras& jogo;
};







// Main function
int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);

    // Cria os objetos Jogador
    // 
    // O método emplace_back() do vetor jogadores_objs
    // cria os objetos diretamente no vetor, evitando cópias desnecessárias.
    // Os argumentos de emplace_back() são passados para o construtor de Jogador.
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    // Cria as threads dos jogadores
    //
    // Semelhante ao que foi feito pra criar os objetos Jogador,
    // mas aqui o construtor da thread recebe o método joga() de cada jogador
    // como argumento, bem como o objeto Jogador no qual o método joga() vai ser chamado.
    std::vector<std::thread> jogadores;
    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    // Esperar pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Esperar pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}

