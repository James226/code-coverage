using System;

namespace CodeCoverage.Example
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine(Hello());
        }

        static string Hello()
        {
            return "Hello World";
        }

        static string Unused()
        {
            return "Unused";
        }
    }

    public class UnusedClass
    {
        public string ExtraUnused()
        {
            return string.Empty;
        }

    }
}
