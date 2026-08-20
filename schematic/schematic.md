graph TD
    %% Farb- und Design-Stile definieren
    classDef kfz/fzg fill:#f9f,stroke:#333,stroke-width:2px;
    classDef bauteil fill:#bbf,stroke:#333,stroke-width:1px;
    classDef schutz fill:#ffb,stroke:#333,stroke-width:2px;
    classDef gnd fill:#ccc,stroke:#333,stroke-width:1px;
    classDef esp fill:#fbb,stroke:#333,stroke-width:2px;

    %% ----------------------------------------------------
    %% ABSCHNITT 1: Kfz-Spannungsversorgung (12V zu 5V)
    %% ----------------------------------------------------
    subgraph Spannungsversorgung [1. Kfz-Spannungsversorgung 12V zu 5V]
        Kl15[T3 Zündungsplus Kl. 15]:::kfz/fzg --> F1[F1: Sicherung 250mA flink]:::bauteil
        F1 --> D1[D1: Verpolungsschutz Diode 1N4007]:::bauteil
        D1 --> DC_IN[DC-DC Step-Down IN +]:::bauteil
        
        %% Schutzkomponenten am Regler-Eingang
        C2[C2: Elko 100uF 50V Puffer]:::schutz
        C3[C3: Keramik 100nF Filter]:::schutz
        D2[D2: TVS-Diode 1.5KE24A Spannungsspitzen]:::schutz
        
        D1 ----> C2 & C3 & D2
        C2 & C3 & D2 ----> Kl31[T3 Masse Kl. 31]:::gnd
        Kl31 --> DC_GND[DC-DC Step-Down IN - / GND]:::gnd
        
        DC_OUT[DC-DC Step-Down OUT + 5.0V]:::bauteil --> VCC_5V[VCC_Clean 5.0V Schiene]:::bauteil
    end

    %% ----------------------------------------------------
    %% ABSCHNITT 2: Signaleingang & OpAmp (3x Verstärkung)
    %% ----------------------------------------------------
    subgraph Signal_und_OpAmp [2. Lambdasonden-Eingang & OpAmp 3x]
        Sonde[T3 Lambdasonde Schwarzes Kabel]:::kfz/fzg --> R1[R1: Eingangswiderstand 10 kOhm]:::schutz
        R1 --> OpAmp_IN_PLUS[OpAmp MCP6002: Pin IN+]:::bauteil
        
        %% Filter vor dem OpAmp Eingang
        C4[C4: Folienkondensator 100nF Tiefpass]:::schutz
        R1 ----> C4 ---> GND_Stern[Zentraler GND Sternpunkt]:::gnd
        
        %% OpAmp Spannungsversorgung & lokale Glättung
        VCC_5V --> OpAmp_VCC[OpAmp MCP6002: Pin VCC 5V]:::bauteil
        C5[C5: Folienkondensator 100nF Glättung]:::schutz
        VCC_5V ----> C5 ---> GND_Stern
        
        %% OpAmp Ausgang und Rückkopplung (Verstärkung einstellen)
        OpAmp_OUT[OpAmp MCP6002: Pin OUT]:::bauteil --> R3[R3: Widerstand 20 kOhm]:::bauteil
        R3 --> OpAmp_IN_MINUS[OpAmp MCP6002: Pin IN-]:::bauteil
        OpAmp_IN_MINUS --> R2[R2: Widerstand 10 kOhm]:::bauteil
        R2 --> GND_Stern
        
        OpAmp_GND[OpAmp MCP6002: Pin GND]:::gnd --> GND_Stern
    end

    %% ----------------------------------------------------
    %% ABSCHNITT 3: ESP32-Eingangsschutz & Mikrocontroller
    %% ----------------------------------------------------
    subgraph ESP32_Schutz [3. ESP32-Eingangsschutz & MCU]
        OpAmp_OUT --> R4[R4: Schutzwiderstand 2.2 kOhm]:::schutz
        
        %% Schutzkomponenten direkt vor dem ESP32 Pin
        D3[D3: Schottky-Diode BAT41 Klemmung]:::schutz
        C6[C6: Folienkondensator 100nF Glättung]:::schutz
        
        R4 ----> D3 & C6
        C6 ---> GND_Stern
        D3 ---> ESP_3V3[ESP32: Pin 3V3]:::esp
        
        %% Signalübergabe an den ADC
        R4 --> ESP_ADC[ESP32: ADC Pin z.B. GPIO 34]:::esp
        
        %% Hauptstromversorgung ESP32
        VCC_5V --> ESP_5V[ESP32: Pin 5V / VIN]:::esp
        ESP_GND[ESP32: Pin GND]:::gnd --> GND_Stern
    end

    %% Globale Masseverbindung zum Fahrzeug
    GND_Stern --> Kl31