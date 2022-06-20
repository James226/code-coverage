using System;
using System.Threading.Tasks;

namespace CodeCoverage.Example
{
    class Program
    {
        static void Main(string[] args)
        {
            var foo = new {Test = "1"};
            Console.WriteLine(Hello());
            Console.WriteLine(OhDear().Result);
        }

        static string Hello()
        {
            return "Hello World";
        }

        static string Unused()
        {
            return "Unused";
        }

        static async Task<string> OhDear()
        {
            return await Task.FromResult("The answer");
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
