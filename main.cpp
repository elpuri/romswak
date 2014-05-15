#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDebug>
#include <math.h>

QString getStringArg(QCoreApplication& a, QString argName) {
    int i = a.arguments().indexOf(argName);
    if (i > 0)
        return a.arguments().at(i + 1);
    return QString();
}


int getIntArg(QCoreApplication& a, QString argName) {
    int i = a.arguments().indexOf(argName);
    if (i > 0)
        return a.arguments().at(i + 1).toInt();
    return 0;
}

double getFloatArg(QCoreApplication& a, QString argName) {
    int i = a.arguments().indexOf(argName);
    if (i > 0)
        return a.arguments().at(i + 1).toDouble();
    return 0.0;
}

void usage() {
    qDebug() << "Usage: romswak sine -width <word width> -length <length in words> [-amplitude <wave amplitude>] -o <output file> [-signed] [-mif]";
    qDebug() << "       romswak data <input file,[offset],[length]> [<input file,[offset],[length]>]... -width <word width> -o <output file> [-mif]";
}

void writeMifHeader(QCoreApplication& a, QTextStream& out, int wordCount, int wordWidth) {
    out << "-- ";
    bool first = true;
    foreach(QString arg, a.arguments()) {
        if (first) {
            out << "romswak ";
            first = false;
        } else
            out << arg << " ";
    }
    out << endl << endl;
    out << "DEPTH = " << wordCount << ";" << endl;
    out << "WIDTH = " << wordWidth << ";" << endl;
    out << "ADDRESS_RADIX = DEC" << ";" << endl;
    out << "DATA_RADIX = BIN" << ";" << endl << endl;

    out << "CONTENT" << endl << "BEGIN" << endl;
}

void writeMifFooter(QTextStream& out)
{
    out << "END;" << endl;
}

QString intToBin(int a, int width) {
    QString bin;
    for (int i = 0; i < width; i++) {
        bin.prepend(a & 1 ? "1" : "0");
        a = a >> 1;
    }
    return bin;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString outputFilename = getStringArg(a, "-o");
    if (outputFilename.isEmpty()) {
        usage();
        qDebug() << "No output file specified!";
        return 0;
    }

    QFile outputFile(outputFilename);
    outputFile.open(QFile::WriteOnly);
    if (!outputFile.isOpen()) {
        qDebug() << "Couldn't open output file" << outputFilename;
        return 0;
    }

    QTextStream output(&outputFile);

    if (a.arguments().at(1) == "sine") {
        int width = getIntArg(a, "-width");
        if (width < 2) {
            qDebug() << "Sine mode: No valid word width defined!";
            return 0;
        }

        int length = getIntArg(a, "-length");
        if (length == 0) {
            qDebug() << "Sine mode: No valid length defined!";
            return 0;
        }

        bool doSigned = a.arguments().contains("-signed");

        int scale = (1 << width) - 2;
        int amplitude = getIntArg(a, "-amplitude");
        if (amplitude == 0) {
            qDebug() << "Defaulting to half of word width:" << scale / 2;
        } else
            scale = amplitude * 2;

        float offset = getFloatArg(a, "-offset");

        if (a.arguments().contains("-mif")) {
            writeMifHeader(a, output, length, width);
            output << endl;

            double phase = 0.0;
            for (int i = 0; i < length; i++) {
                double value = 0.5 * sin(phase) + offset + (doSigned ? 0.0 : 0.5);
                int intValue = floor(value * scale) + (doSigned ? 0 : 1);

                output << i << " : " << intToBin(intValue, width) << ";       -- " << intValue << endl;
                phase += 2.0 * M_PI / length;
            }
            output << endl;
            writeMifFooter(output);
        } else {
            // Write raw
            int outputWordBytes;
            if (width <= 8)
                outputWordBytes = 1;
            else if(width <= 16)
                outputWordBytes = 2;
            else if(width <= 24)
                outputWordBytes = 3;
            else if(width <= 32)
                outputWordBytes = 4;
            double phase = 0.0;
            for (int i = 0; i < length; i++) {
                double value = 0.5 * sin(phase) + offset + (doSigned ? 0.0 : 0.5);
                int intValue = floor(value * scale) + (doSigned ? 0 : 1);

                // write big endian
                for (int j = 0; j < outputWordBytes; j++) {
                    char byte = (intValue >> ((outputWordBytes - j - 1) * 8)) & 0xFF;
                    outputFile.write(&byte, 1);
                }

                phase += 2.0 * M_PI / length;
            }
        }


    } else if (a.arguments().at(1) == "data") {
        bool mifOutput = a.arguments().contains("-mif");

        int width = getIntArg(a, "-width");
        if (width == 0 && mifOutput) {
            qDebug() << "Data mode: No valid word width defined with MIF output. Defaulting to 8-bit width.";
            width = 8;
        }

        int argIndex = 2;
        QByteArray data;
        while (!a.arguments().at(argIndex).startsWith("-")) {
            QStringList inputFileArg = a.arguments().at(argIndex).split(",");
            QString filename = inputFileArg[0];
            QFile file(filename);
            if (!file.open(QFile::ReadOnly)) {
                qDebug() << "Can't open input file" << filename;
                return 0;
            }


            // Figure out offset. Default to 0 if offset is not specified.
            int offset = 0;
            if (inputFileArg.length() > 1) {
                bool ok;
                offset = inputFileArg.at(1).toInt(&ok);
                if (!ok) {
                    qDebug() << "Invalid offset" << inputFileArg.at(1) << "for inputfile" << filename;
                    return 0;
                }
            }

            // Figure out length. Default to file length if length is not specified.
            qint64 length;
            if (inputFileArg.length() < 3)
                length = file.size() - offset;
            else {
                bool ok;
                length = inputFileArg.at(2).toLongLong(&ok);
                if (!ok) {
                    qDebug() << "Invalid length" << inputFileArg.at(2) << "for inputfile" << filename;
                    return 0;
                }
            }

            qDebug() << "Reading" << filename << offset << length;
            if (offset != 0)
                file.seek(offset);

            data.append(file.read(length));

            file.close();
            argIndex++;
        }

        if (mifOutput) {

            int inputWordBytes;
            if (width <= 8)
                inputWordBytes = 1;
            else if(width <= 16)
                inputWordBytes = 2;
            else if(width <= 24)
                inputWordBytes = 3;
            else if(width <= 32)
                inputWordBytes = 4;
            else {
                qDebug() << "Data mode: max 32-bit wide word width allowed!";
                return 0;
            }

            if (data.length() % inputWordBytes != 0) {
                qDebug() << "Data mode: file length" << data.length() << "is not divisible by input word size (in bytes)" << inputWordBytes << "!";
                return 0;
            }

            int wordCount = data.length() / inputWordBytes;

            writeMifHeader(a, output, wordCount, width);

            int byteIndex = 0;
            for (int i = 0; i < wordCount; i++) {
                int word = 0;
                // Read big endian
                for (int j=0; j < inputWordBytes; j++) {
                    word = word << 8;
                    word |= (unsigned char) data.at(byteIndex);
                    byteIndex++;
                }
                output << i << " : " << intToBin(word, width) << ";" << endl;
            }
            writeMifFooter(output);
        } else
            outputFile.write(data);

    }  else {
        qDebug() << "Unknown operation mode";
        usage();
        return 0;
    }

    outputFile.close();
}
