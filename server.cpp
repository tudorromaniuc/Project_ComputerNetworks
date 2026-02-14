#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm> 
#include <random>    
#include <ctime>
#include <iostream>
#include <sstream>

using namespace std;

#define PORT 2023

struct UserSesiune {
    string nume;
    bool este_admin;
    bool logat;
};

void scrie_log(string fisier, string info) {
    ofstream f(fisier, ios::app);
    if (f.is_open()) {
        f << info << endl;
        f.close();
    }
}

void trimite_email(string destinatar, string subiect, string mesaj) {
    ofstream f("server_emails.txt", ios::app);
    f << "TO: " << destinatar << " | SUBJ: " << subiect << " | MSG: " << mesaj << endl;
    f.close();
}

int get_next_match_id() {
    ifstream f("db_meciuri.txt");
    string linie;
    int max_id = 0;
    while(getline(f, linie)) {
        size_t pos = linie.find("|");
        if(pos != string::npos) {
            int id = atoi(linie.substr(0, pos).c_str());
            if(id > max_id) max_id = id;
        }
    }
    f.close();
    return max_id + 1;
}

void generare_meciuri(int id_camp, int nr_max, string nume_joc) {
    ifstream fin("db_inscrieri.txt");
    vector<string> jucatori;
    string linie;
    
    while (getline(fin, linie)) {
        stringstream ss(linie);
        string seg;
        vector<string> tokens;
        while(getline(ss, seg, '|')) tokens.push_back(seg);
        
        if(tokens.size() >= 2) {
            if(atoi(tokens[0].c_str()) == id_camp) {
                jucatori.push_back(tokens[1]);
            }
        }
    }
    fin.close();

    if (jucatori.size() < (size_t)nr_max) return;

    random_device rd;
    mt19937 g(rd());
    shuffle(jucatori.begin(), jucatori.end(), g);

    ofstream fout("db_meciuri.txt", ios::app);
    
    for (size_t i = 0; i < jucatori.size(); i += 2) {
        if (i + 1 >= jucatori.size()) break;
        
        int mid = get_next_match_id();
        string p1 = jucatori[i];
        string p2 = jucatori[i+1];

        fout << mid << "|" << id_camp << "|1|" << p1 << "|" << p2 << "|0|0|Maine_20:00" << endl;

        trimite_email(p1, "Meci Nou", "Adversar: " + p2);
        trimite_email(p2, "Meci Nou", "Adversar: " + p1);
    }
    fout.close();
}

void handle_client(int conn) {
    char buff[2048];
    UserSesiune user = {"", false, false};

    while (true) {
        memset(buff, 0, sizeof(buff));
        if (recv(conn, buff, sizeof(buff)-1, 0) <= 0) {
            close(conn);
            exit(0);
        }

        string comanda_raw = string(buff);
        while (!comanda_raw.empty() && (comanda_raw.back() == '\n' || comanda_raw.back() == '\r')) 
            comanda_raw.pop_back();

        stringstream ss(comanda_raw);
        string cmd;
        ss >> cmd;

        if (cmd == "REGISTER") {
            string u, p, m;
            ss >> u >> p >> m;
            if (u.empty() || p.empty() || m.empty()) {
                send(conn, "Eroare: REGISTER user pass mail\n", 30, 0);
            } else {
                ofstream f("db_users.txt", ios::app);
                f << u << "|" << p << "|" << m << "|0" << endl; 
                f.close();
                user.nume = u;
                user.este_admin = false;
                user.logat = true;
                send(conn, "Inregistrat si logat ca User.\n", 29, 0);
            }
        }
        else if (cmd == "LOGIN") {
            string u, p;
            ss >> u >> p;
            ifstream f("db_users.txt");
            string linie;
            bool gasit = false;
            while (getline(f, linie)) {
                stringstream sl(linie);
                string fu, fp, fm, fr;
                getline(sl, fu, '|');
                getline(sl, fp, '|');
                getline(sl, fm, '|');
                getline(sl, fr, '|');
                
                if (fu == u && fp == p) {
                    user.nume = u;
                    user.logat = true;
                    user.este_admin = (fr == "1");
                    gasit = true;
                    break;
                }
            }
            f.close();
            if (gasit) {
                string msg = "Logat cu succes. Rol: " + string(user.este_admin ? "ADMIN" : "USER") + "\n";
                send(conn, msg.c_str(), msg.length(), 0);
            } else {
                send(conn, "User sau parola gresite.\n", 24, 0);
            }
        }
        else if (cmd == "ADAUGA_CAMP") {
            if (!user.logat || !user.este_admin) {
                send(conn, "Doar adminii pot adauga campionate.\n", 36, 0);
            } else {
                string numec, joc, tip;
                int maxp;
                ss >> numec >> joc >> maxp >> tip;
                if (numec.empty()) {
                    send(conn, "Sintaxa: ADAUGA_CAMP nume joc nr tip\n", 37, 0);
                } else {
                    int id = time(NULL) % 10000;
                    ofstream f("db_campionate.txt", ios::app);
                    f << id << "|" << numec << "|" << joc << "|" << maxp << "|" << tip << "|OPEN" << endl;
                    f.close();
                    char r[100];
                    sprintf(r, "Campionat %d creat.\n", id);
                    send(conn, r, strlen(r), 0);
                }
            }
        }
        else if (cmd == "LISTA") {
            if (!user.logat) {
                send(conn, "Trebuie sa fii logat.\n", 22, 0);
            } else {
                ifstream f("db_campionate.txt");
                string linie, raspuns = "ID | Nume | Joc | Status\n------------------------\n";
                while(getline(f, linie)) {
                    raspuns += linie + "\n";
                }
                f.close();
                if(raspuns.length() < 50) raspuns = "Nu exista campionate.\n";
                send(conn, raspuns.c_str(), raspuns.length(), 0);
            }
        }
        else if (cmd == "JOIN") {
            if (!user.logat) {
                send(conn, "Trebuie sa fii logat.\n", 22, 0);
            } else {
                int id_req;
                if (ss >> id_req) {
                    ofstream f("db_inscrieri.txt", ios::app);
                    f << id_req << "|" << user.nume << endl;
                    f.close();
                    
                    ifstream fc("db_campionate.txt");
                    string l;
                    while(getline(fc, l)) {
                        stringstream sl(l);
                        string seg;
                        vector<string> d;
                        while(getline(sl, seg, '|')) d.push_back(seg);
                        if(d.size() > 3 && atoi(d[0].c_str()) == id_req) {
                            generare_meciuri(id_req, atoi(d[3].c_str()), d[2]);
                            break;
                        }
                    }
                    fc.close();
                    send(conn, "Te-ai inscris!\n", 15, 0);
                } else {
                    send(conn, "Sintaxa: JOIN <id>\n", 19, 0);
                }
            }
        }
        else if (cmd == "SET_SCOR") {
            if (!user.logat) {
                send(conn, "Acces interzis.\n", 16, 0);
            } else {
                int mid, s1, s2;
                if(ss >> mid >> s1 >> s2) {
                    ifstream f("db_meciuri.txt");
                    vector<string> linii;
                    string l;
                    bool gasit = false;
                    while(getline(f, l)) {
                        size_t pos = l.find("|");
                        int cur_id = atoi(l.substr(0, pos).c_str());
                        if(cur_id == mid) {
                            if (!user.este_admin && l.find(user.nume) == string::npos) {
                                linii.push_back(l); 
                                continue; 
                            }
                            stringstream sl(l);
                            string seg;
                            vector<string> t;
                            while(getline(sl, seg, '|')) t.push_back(seg);
                            
                            string noua_linie = t[0] + "|" + t[1] + "|" + t[2] + "|" + t[3] + "|" + t[4] + "|" + to_string(s1) + "|" + to_string(s2) + "|" + t[7];
                            linii.push_back(noua_linie);
                            gasit = true;
                        } else {
                            linii.push_back(l);
                        }
                    }
                    f.close();
                    
                    if(gasit) {
                        ofstream fo("db_meciuri.txt");
                        for(auto &x : linii) fo << x << endl;
                        fo.close();
                        send(conn, "Scor actualizat.\n", 17, 0);
                    } else {
                        send(conn, "Meciul nu a fost gasit sau nu ai dreptul.\n", 42, 0);
                    }
                } else {
                    send(conn, "Sintaxa: SET_SCOR <id_meci> <s1> <s2>\n", 38, 0);
                }
            }
        }
        else if (cmd == "ISTORIC") {
            if(!user.logat || !user.este_admin) {
                send(conn, "Doar adminii pot vedea istoricul.\n", 34, 0);
            } else {
                ifstream f("db_meciuri.txt");
                string l, rasp = "ID | Camp | Runda | P1 vs P2 | Scor | Data\n";
                while(getline(f, l)) rasp += l + "\n";
                f.close();
                send(conn, rasp.c_str(), rasp.length(), 0);
            }
        }
        else if (cmd == "REPROGRAMEAZA") {
            if(!user.logat) {
                send(conn, "Trebuie sa fii logat.\n", 22, 0);
            } else {
                string data_noua;
                ss >> data_noua;
                if(data_noua.empty()) {
                    send(conn, "Sintaxa: REPROGRAMEAZA <data_ora>\n", 33, 0);
                } else {
                    ifstream f("db_meciuri.txt");
                    vector<string> linii;
                    string l;
                    bool modif = false;
                    while(getline(f, l)) {
                        if(l.find(user.nume) != string::npos) { 
                            size_t last_sep = l.find_last_of("|");
                            string base = l.substr(0, last_sep);
                            linii.push_back(base + "|" + data_noua);
                            modif = true;
                        } else {
                            linii.push_back(l);
                        }
                    }
                    f.close();
                    if(modif) {
                        ofstream fo("db_meciuri.txt");
                        for(auto &x : linii) fo << x << endl;
                        fo.close();
                        send(conn, "Meci reprogramat.\n", 18, 0);
                    } else {
                        send(conn, "Nu ai meciuri active.\n", 22, 0);
                    }
                }
            }
        }
        else if (cmd == "LOGOUT") {
            user.logat = false;
            user.nume = "";
            user.este_admin = false;
            send(conn, "Deconectat.\n", 12, 0);
        }
        else {
            send(conn, "Comanda invalida. Scrie HELP.\n", 30, 0);
        }
    }
}

int main() {
    int sd, client;
    struct sockaddr_in s_addr, c_addr;
    socklen_t l = sizeof(c_addr);

    sd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) {
        perror("Bind error"); return 1;
    }

    listen(sd, 5);
    printf("Server pornit pe port %d. Astept clienti...\n", PORT);
    
    signal(SIGCHLD, SIG_IGN);

    while(1) {
        client = accept(sd, (struct sockaddr*)&c_addr, &l);
        printf("Client conectat.\n");
        if(fork() == 0) {
            close(sd);
            handle_client(client);
            exit(0);
        }
        close(client);
    }
    return 0;
}